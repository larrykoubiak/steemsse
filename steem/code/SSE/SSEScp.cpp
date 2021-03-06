#include "SSE.h"

#if defined(SSE_DISK_SCP)
/*  The SCP interface is based on the STW interface, so that integration in
    Steem (disk manager, FDC commands...) is straightforward.
    Remarkably few changes are necessary in the byte-level WD1772 emu to have
    some SCP images loading.
    For some others, however, we had to graft bit-level algorithms (3rd party
    -inspired).
    Up to date it seems Steem SSE can run all SCP images, with some difficulties
    on weak bit protections.
*/

#include "../pch.h"
#include <conditions.h>
#include <cpu.decla.h>
#include <fdc.decla.h>
#include <floppy_drive.decla.h>
#include <iorw.decla.h>
#include <mfp.decla.h>
#include <psg.decla.h>
#include <run.decla.h>

#include "SSECpu.h"
#include "SSEInterrupt.h"
#include "SSEShifter.h"
#include "SSEDecla.h"
#include "SSEDebug.h"
#include "SSEFloppy.h"
#include "SSEOption.h"
#include "SSEScp.h"

#define N_SIDES FloppyDrive[Id].Sides
#define N_TRACKS FloppyDrive[Id].TracksPerSide

#if !defined(BIG_ENDIAN_PROCESSOR)
#include <acc.decla.h>
#define SWAP_WORD(val) *val=change_endian(*val);
#else
#define SWAP_WORD(val)
#endif


TImageSCP::TImageSCP() {
  Init();
}


TImageSCP::~TImageSCP() {
  Close();
}


#define LOGSECTION LOGSECTION_IMAGE_INFO


void TImageSCP::Close() {
  if(fCurrentImage)
  {
    TRACE_LOG("SCP %d close image\n",Id);
    fclose(fCurrentImage);
    if(TimeFromIndexPulse)
      free(TimeFromIndexPulse);
  }
  Init();
}


void  TImageSCP::ComputePosition() {
  // when we start reading/writing, where on the disk?
  
  ASSERT(TimeFromIndexPulse);
  if(!TimeFromIndexPulse)
    return; //safety
  
  int cycles=time_of_next_event-SF314[DRIVE].time_of_last_ip;// CPU cycles since IP
  DWORD units=cycles*5; // in SCP units
  
  Position=0;
  for(DWORD i=0;i<nBits;i++) // slow search
  {
    if(TimeFromIndexPulse[i]>=units)
    {
      Position=i; // can be 0
      break;
    }
  }
#if defined(SSE_DISK_SCP_RANDOMISE)
/* Randomise first bit when starting reading a track.
   Fixes War Heli track 68
   The protection is looking for A1 47 4D 43 then checks that the byte
   before A1 isn't twice the same.
   Using rand(), it still may fail.
   TODO test if it breaks other SCP games?
*/
  if(!Position)
    Position=rand()%4; // IP + non-synchronisation
#endif

#if defined(SSE_WD1772_BIT_LEVEL)
  WD1772.Dpll.Reset(ACT); 
#endif

  ASSERT(SF314[DRIVE].CyclesPerByte());
  // just informative? TODO
  Disk[DRIVE].current_byte=(time_of_next_event-SF314[DRIVE].time_of_last_ip)
    /SF314[DRIVE].CyclesPerByte();
}

#if !defined(SSE_WD1772_BIT_LEVEL)||defined(SSE_DISK_SCP_TO_MFM_PREVIEW)

BYTE TImageSCP::GetDelay(int position) {
  // we want delay in ms, typically 4, 6, 8
  WORD units_to_next_flux=UnitsToNextFlux(position);
  BYTE delay_in_us;
  delay_in_us=UsToNextFlux(units_to_next_flux);
  return delay_in_us;    
}

#endif


int TImageSCP::UnitsToNextFlux(DWORD position) {
  // 1 unit = 25 nanoseconds = 1/40 ms
  ASSERT(position<nBits);
  ASSERT(position>=0);
  position=position%nBits; // safety
  DWORD time1=0,time2;
  if(position)
    time1=TimeFromIndexPulse[position-1];
  time2=TimeFromIndexPulse[position];
  ASSERT( time2>time1 );
  int units_to_next_flux=time2-time1; 
#if defined(SSE_DISK_SCP_DRIVE_WOBBLE) // this takes care of weak bits (?)
  int wobble=(rand()%4)-2; // a bit simplistic? 
  units_to_next_flux+=wobble;
#endif
  return units_to_next_flux;    
}


int TImageSCP::UsToNextFlux(int units_to_next_flux) {
  BYTE us_to_next_flux;
  BYTE ref_us= ((units_to_next_flux/40)+1)&0xFE;  // eg 4
  WORD ref_units = ref_us * 40;
  if(units_to_next_flux<ref_units-SCP_DATA_WINDOW_TOLERANCY)
    us_to_next_flux=ref_us-1;
  else if (units_to_next_flux>ref_units+SCP_DATA_WINDOW_TOLERANCY)
    us_to_next_flux=ref_us+1;
  else
    us_to_next_flux=ref_us;
  return us_to_next_flux;    
}


WORD TImageSCP::GetMfmData(WORD position) {
/*  We use the same interface for SCP as for STW so that integration
    with the Disk manager, WD1772 emu etc. is straightforward.
    But precise emulation doesn't send MFM data word by word (16bit).
    Instead it sends bytes and AM signals according to bit sequences,
    as analysed in (3rd party-inspired) WD1772.ShiftBit().
    note we need SSE_WD1772_BIT_LEVEL, we didn't keep beta code in v3.7.1
*/

  WORD mfm_data=0;

  if(!TimeFromIndexPulse) //safety, SCP track in ram?
    return mfm_data;

  // must compute new starting point?
  if(position!=0xFFFF)
    ComputePosition();

#if defined(SSE_WD1772_BIT_LEVEL)

  // we manage timing here, maybe we should do that in WD1772 instead
  int a1=WD1772.Dpll.ctime,a2,tm=0;

  // clear dsr signals
  WD1772.Amd.aminfo&=~(CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRAM|CAPSFDC_AI_DSRMA1);

  // loop until break
  for(int i=0; ;i++) 
  {
    int bit=WD1772.Dpll.GetNextBit(tm,DRIVE); //tm isn't used...
    ASSERT(bit==0 || bit==1); // 0 or 1, clock and data
    TRACE_MFM("%d",bit); // full flow of bits
    if(WD1772.ShiftBit(bit)) // true if byte ready to transfer
      break;
  }//nxt i

  //WD1772.Mfm.data_last_bit=(mfm_data&1); // no use

  a2=WD1772.Dpll.ctime;
  int delay_in_cycles=(a2-a1);

#ifdef SSE_DEBUG  // only report DPLL if there's some adjustment
  if(WD1772.Dpll.increment!=128|| WD1772.Dpll.phase_add||WD1772.Dpll.phase_sub
    ||WD1772.Dpll.freq_add||WD1772.Dpll.freq_sub)
  {
    ASSERT( !(WD1772.Dpll.freq_add && WD1772.Dpll.freq_sub) ); 
    ASSERT( !(WD1772.Dpll.phase_add && WD1772.Dpll.phase_sub) );
    TRACE_MFM(" DPLL (%d,%d,%d) ",WD1772.Dpll.increment,WD1772.Dpll.phase_add-WD1772.Dpll.phase_sub,WD1772.Dpll.freq_add-WD1772.Dpll.freq_sub);
  }
  ASSERT(delay_in_cycles>0);
  TRACE_MFM(" %d cycles\n",delay_in_cycles);
#endif

  WD1772.update_time=time_of_next_event+delay_in_cycles; 

  if(WD1772.update_time-ACT<=0) // safety
  {
    TRACE_LOG("Argh! wrong disk timing %d ACT %d diff %d last IP %d pos %d/%d delay %d units %d\n",
      WD1772.update_time,ACT,ACT-WD1772.update_time,SF314[DRIVE].time_of_last_ip,Position,nBits-1,delay_in_cycles,TimeFromIndexPulse[Position-1]);
    WD1772.update_time=ACT+SF314[DRIVE].cycles_per_byte;
  }
#endif

  ASSERT(!mfm_data); // see note at top of function
  mfm_data=WD1772.Mfm.encoded; // correct?
  return mfm_data;
}


int TImageSCP::GetNextTransition(BYTE& us_to_next_flux) {
  int t=UnitsToNextFlux(Position);
  us_to_next_flux=UsToNextFlux(t); // in parameter
  IncPosition();
  t/=5; // in cycles
  return t; 
}


void TImageSCP::IncPosition() {
  ASSERT( Position>=0 );
  ASSERT( Position<nBits );
  Position++;
#if defined(SSE_DISK_SCP_394) //safer
  if(Position>=nBits)
#else
  if(Position==nBits)
#endif
  {
    Position=0;
    TRACE_FDC("\nSCP triggers IP side %d track %d rev %d/%d\n",
      CURRENT_SIDE,floppy_head_track[DRIVE],rev+1,file_header.IFF_NUMREVS);
    
/*  If a sector is spread over IP, we make sure that our event
    system won't start a new byte before returning to current
    byte. 
*/
    SF314[DRIVE].IndexPulse(true); // true for "image triggered"

    // provided there are >1 revs...    
    if(file_header.IFF_NUMREVS>1)
    {
      // we step revs 0->1 each IP, we'll reload 0 during rev
      // works with Turrican, I Ludicrus, Leavin' Teramis
      // Notice we do no computing, the first bit of the new rev
      // is relative to last bit of previous rev, or we are very
      // lucky.
      LoadTrack(CURRENT_SIDE,SF314[DRIVE].Track(),true);
    }      
  }
}


void TImageSCP::Init() {
  fCurrentImage=NULL;
  TimeFromIndexPulse=NULL;
  N_SIDES=2;
  N_TRACKS=83; //max
  nBytes=DISK_BYTES_PER_TRACK; //not really pertinent (TODO?)
}


#if defined(SSE_BOILER) && defined(SSE_DISK_SCP_TO_MFM_PREVIEW)
/*  This function was used for development of a flux to MFM decoder.
    It transforms raw flux reversal delays into MFM, all at once, and
    "syncs" when it detects $A1 address marks.
    It is commanded by 'log image info' + 'mfm' in the control mask browser.
    It computes the # data bytes on the track, it's generally more than 6250,
    but that could be overrated.
    It still helped me probe Turrican.
*/

void TImageSCP::InterpretFlux() {
  WORD current_mfm_word=0,amd=0,ndatabytes=0;
  int bit_counter=0;
  for(int i=0;i<nBits;i++)
  {
    BYTE delay_in_us=GetDelay(i);
#if defined(SSE_BOILER_TRACE_CONTROL)
    if(TRACE_MASK3&TRACE_CONTROL_FDCMFM)
      TRACE_LOG("%d ",delay_in_us);
#endif
    int n_shifts=(delay_in_us/2); // 4->2; 6->3; 8->4
    for(int j=0;j<n_shifts;j++)
    {
      if(j==n_shifts-1) // eg 001 3rd iteration we set bit
      {
        current_mfm_word|=1;
        amd|=1;
      }
      if(amd==0x4489 || amd==0x5224) 
      {
#if defined(SSE_BOILER_TRACE_CONTROL)
        if(TRACE_MASK3&TRACE_CONTROL_FDCMFM)
          TRACE_LOG("AM? %x ctr %d\n",amd,bit_counter);
#endif
        if(amd==0x4489)
        {
          amd=0; // no overlap
          current_mfm_word<<=15-bit_counter;
          bit_counter=15; //sync
        }
      }
      amd<<=1;
      bit_counter++;
      if(bit_counter==16)
      {
        WD1772.Mfm.encoded=current_mfm_word;
        WD1772.Mfm.Decode();
#if defined(SSE_BOILER_TRACE_CONTROL)
        if(TRACE_MASK3&TRACE_CONTROL_FDCMFM)
          TRACE_LOG("MFM %X amd %X C %X D %X \n",current_mfm_word,amd,WD1772.Mfm.clock,WD1772.Mfm.data);
#endif
        current_mfm_word=0;
        bit_counter=0;
        ndatabytes++;
      }
      current_mfm_word<<=1;
    }//j
  }//i
  TRACE_LOG("%d bytes on SCP track\n",ndatabytes);
}

#endif


bool TImageSCP::LoadTrack(BYTE side,BYTE track,bool reload) {
  ASSERT(Id==0||Id==1);
  bool ok=false;

  ASSERT( side<2 && track<N_TRACKS ); // unique side may be 1
  if(side>=2 || track>=N_TRACKS)
    return ok; //no crash
  BYTE trackn=track;
  if(N_SIDES==2) // general case
    trackn=track*2+side; 
#if defined(SSE_DISK_SCP_394) // don't reload if only 1 (perfect) rev
  if(track_header.TDH_TRACKNUM==trackn //already loaded
    && !rev && (!reload||file_header.IFF_NUMREVS==1))
    return true;
#else
  if(!rev &&! reload && track_header.TDH_TRACKNUM==trackn) //already loaded
    return true;
#endif
  if(TimeFromIndexPulse) 
    free(TimeFromIndexPulse);
  TimeFromIndexPulse=NULL;

  int offset=file_header.IFF_THDOFFSET[trackn]; // base = start of file

  if(fCurrentImage) // image exists
  {  
    fseek(fCurrentImage,offset,SEEK_SET);
    int size=sizeof(TSCP_track_header);
    fread(&track_header,size,1,fCurrentImage);

/*  Determine which track rev to load.
    Turrican SCP will fail if we don't start on rev1 so that it reads
    sector data of rev2 over IP (index pulse).
    This is because IP is by nature imprecise, and it won't fall on the same
    bitcell each time (unless you have mastering hardware, not mama's drive).
    One rev from IP to IP, unless "doctored", may have one or two bits too
    few or too many at the beginning or the end.
    Doing rev1 -> rev2 ensures that we use the exact same timing for IP,
    and no bit is lost or gained.
    It will work with 1rev versions if the copy is 100% (not our 
    responsibility) or if data at IP isn't important (most cases).
*/
    if(reload)
      rev++;
    else
      rev=0;
    rev%=file_header.IFF_NUMREVS;

    WORD* flux_to_flux_units_table_16bit=(WORD*)calloc(track_header.\
      TDH_TABLESTART[rev].TDH_LENGTH,sizeof(WORD));
    TimeFromIndexPulse=(DWORD*)calloc(track_header.TDH_TABLESTART[rev].\
      TDH_LENGTH,sizeof(DWORD));
    ASSERT(flux_to_flux_units_table_16bit && TimeFromIndexPulse);

    if(flux_to_flux_units_table_16bit && TimeFromIndexPulse)
    {
      fseek(fCurrentImage,offset+track_header.TDH_TABLESTART[rev].TDH_OFFSET,
        SEEK_SET);

      fread(flux_to_flux_units_table_16bit,sizeof(WORD),track_header.\
        TDH_TABLESTART[rev].TDH_LENGTH,fCurrentImage);

      int units_from_ip=0;
      nBits=0;
      // reverse endianess and convert to time after IP, one data per bit (SLOW)
      for(DWORD i=0;i<track_header.TDH_TABLESTART[rev].TDH_LENGTH;i++)
      {
        SWAP_WORD( &flux_to_flux_units_table_16bit[i] );
        ASSERT(units_from_ip + flux_to_flux_units_table_16bit[i] >= units_from_ip);
        units_from_ip+=(flux_to_flux_units_table_16bit[i])
          ? flux_to_flux_units_table_16bit[i] : 0xFFFF;
        ASSERT(units_from_ip<0x7FFFFFFF); // max +- 200,000,000, OK
        if(flux_to_flux_units_table_16bit[i])
          TimeFromIndexPulse[nBits++]=units_from_ip;
      }
#if !defined(SSE_DISK_SCP_394) // we don't always load at IP
      Position=0;
#endif
      free(flux_to_flux_units_table_16bit);
      ok=true;
    }

    TRACE_LOG("SCP LoadTrack side %d track %d %c%c%c %d rev %d/%d  \
INDEX TIME %d (%f ms) TRACK LENGTH %d bits %d last bit unit %d DATA \
OFFSET %d  checksum %X\n",
side,track,
track_header.TDH_ID[0],track_header.TDH_ID[1],track_header.TDH_ID[2],
track_header.TDH_TRACKNUM,rev+1,file_header.IFF_NUMREVS,
track_header.TDH_TABLESTART[rev].TDH_DURATION,
(float)track_header.TDH_TABLESTART[rev].TDH_DURATION*25/1000000,
track_header.TDH_TABLESTART[rev].TDH_LENGTH, nBits,TimeFromIndexPulse[nBits-1],
track_header.TDH_TABLESTART[rev].TDH_OFFSET,track_header.track_data_checksum);

    Disk[Id].current_side=side;
    Disk[Id].current_track=track;

#if defined(SSE_BOILER) && defined(SSE_DISK_SCP_TO_MFM_PREVIEW)
    InterpretFlux();
#endif
  }

  return ok;
}


bool TImageSCP::Open(char *path) {
  bool ok=false;
  Close(); // make sure previous image is correctly closed
  fCurrentImage=fopen(path,"rb");
  if(fCurrentImage) // image exists
  {
    // we read only the header
    if(fread(&file_header,sizeof(TSCP_file_header),1,fCurrentImage))
    {
      if(!strncmp(DISK_EXT_SCP,(char*)&file_header.IFF_ID,3)) // it's SCP
      {
        // compute N_SIDES and N_TRACKS
        if(file_header.IFF_HEADS)
        {
          N_SIDES=1;
          N_TRACKS=file_header.IFF_END-file_header.IFF_START+1;
        }
        else
          N_TRACKS=(file_header.IFF_END-file_header.IFF_START+1)/2;

#if defined(SSE_DEBUG)
TRACE_LOG("SCP %d sides %d tracks %d IFF_VER %X IFF_DISKTYPE %X IFF_NUMREVS %d \
IFF_START %d IFF_END %d IFF_FLAGS %d IFF_ENCODING %d IFF_HEADS %d \
IFF_RSRVED %X IFF_CHECKSUM %X\n",Id,N_SIDES,N_TRACKS,file_header.IFF_VER,
file_header.IFF_DISKTYPE,file_header.IFF_NUMREVS,file_header.IFF_START,
file_header.IFF_END,file_header.IFF_FLAGS,file_header.IFF_ENCODING,
file_header.IFF_HEADS,file_header.IFF_RSRVED,file_header.IFF_CHECKSUM);
#endif

        track_header.TDH_TRACKNUM=0xFF;
        ok=true; //TODO some checks?
      }//cmp
    }//read
  }

  if(!ok)
    Close();
  else 
    SF314[Id].MfmManager=this;
  return ok;
}

#pragma warning(disable:4100)//unreferenced formal parameter

void TImageSCP::SetMfmData(WORD position, WORD mfm_data) {
  // :) TODO
}

#pragma warning(default:4100)

#undef LOGSECTION

#endif//SCP


