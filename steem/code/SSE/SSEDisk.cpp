#include "SSE.h"

#include "../pch.h"

#if defined(SSE_DISK_EXT) // available for all SSE versions
char *extension_list[]={ "","ST","MSA","DIM","STT","STX","IPF",
"CTR","STG","STW","PRG","TOS","SCP","HFE",""}; //need last?

char buffer[5]=".XXX";

char *dot_ext(int i) { // is it ridiculous? do we reduce or add overhead?
  strcpy(buffer+1,extension_list[i]);
  return buffer;
}

#endif

#if defined(SSE_DISK)

#include <fdc.decla.h>
#include <floppy_drive.decla.h>

#include "SSEDebug.h"
#include "SSEDisk.h"
#include "SSEDrive.h"
#include "SSEFloppy.h"


TDisk::TDisk() {
#ifdef SSE_DEBUG
  Id=0xFF;
  current_byte=0xFFFF;
#endif
  Init();
}


void TDisk::Init() {
  TrackBytes=TRACK_BYTES;
}


WORD TDisk::BytePositionOfFirstId() { // with +7 for reading ID //no!
  return ( PostIndexGap() + ( (nSectors()<11)?12+3+1:3+3+1) );
}


WORD TDisk::BytesToID(BYTE &num) {
/*  Compute distance in bytes between current byte and desired ID
    identified by 'num' (sector)
    return 0 if it doesn't exist
    if num=0, assume next ID, num will contain sector index, 1-based
    (so, not num!)
*/

  short bytes_to_id=0;
  
  const WORD current_byte=SF314[Id].BytePosition();

  if(FloppyDrive[Id].Empty())
    ;
  else
  {
    //here we assume normal ST disk image, sectors are 1...10
    WORD record_length=RecordLength();
    BYTE n_sectors=nSectors();
    WORD byte_first_id=BytePositionOfFirstId();
    WORD byte_target_id;

    // If we're looking for whatever next num, we compute it first
    if(!num)
    {
      num=(current_byte-byte_first_id)/record_length+1; // current
      if(((current_byte)%record_length)>byte_first_id) // only if past ID! (390)
        num++; //next
#if defined(SSE_BUGFIX_393)
      // fix regression v3.9.0: Wipe-Out ; it's a special case, 6 sectors/track
      if(num>=n_sectors+1)
        num=1; 
#else
      if(num==n_sectors+1) num=1; //TODO smart way
#endif
    }

    byte_target_id=byte_first_id+(num-1)*record_length;
    bytes_to_id=byte_target_id-current_byte;
    if(bytes_to_id<0) // passed it
      bytes_to_id+=TrackBytes; // next rev
  }
  return (WORD)bytes_to_id;
}


WORD TDisk::HblsPerSector() {
  return nSectors()?(SF314[Id].HblsPerRotation()-SF314[Id].BytesToHbls(TrackGap()))/nSectors() : 0;
}


void TDisk::NextID(BYTE &RecordIdx,WORD &nHbls) {
/*  This routine was written to improve timing of command 'Read Address',
    used by ProCopy. Since it exists, it is also used for 'Verify'.
    In 'native' mode both.
*/
  RecordIdx=0;
  nHbls=0;
  if(FloppyDrive[Id].Empty())
    return;

  WORD BytesToRun=BytesToID(RecordIdx);
  if(RecordIdx)
    RecordIdx--; //!
  BytesToRun+=7-1; // FE + 6 ID bytes are read (?)

  nHbls=SF314[Id].BytesToHbls(BytesToRun);
#if 0 && defined(SSE_DEBUG)
  BYTE a=0;
  TRACE("Next ID #%d, %d bytes, BytesToID(0) %d\n",RecordIdx,BytesToRun,BytesToID(a));
#endif
}


BYTE TDisk::nSectors() { 
  // should we add IPF, STX? - here we do native - in IPF there's no nSectors,
  // in STX, it's indicative
  BYTE nSects;

  if(FloppyDrive[Id].STT_File)
  {
    FDC_IDField IDList[30]; // much work each time, but STT rare
    nSects=FloppyDrive[Id].GetIDFields(CURRENT_SIDE,SF314[Id].Track(),IDList);
  }
  else
    nSects=FloppyDrive[Id].SectorsPerTrack;
  return nSects;
}


BYTE TDisk::PostIndexGap() {
  switch( nSectors() )
  {
  case 9:
    return 60;
  case 10:
    return 22; //?
  default:
    return 10;
  }
}


BYTE TDisk::PreDataGap() {
  int gap=0;
  switch(nSectors())
  {
  case 9:
  case 10: // with ID (7) and DAM (1)
    gap=12+3+7+22+12+3+1; 
    break;
  case 11:
    gap= 3+3+7+22+12+3+1;
    break;
  }
  return gap;
}


BYTE TDisk::PostDataGap() {
  return (nSectors()<11)? 40+2 : 1+2; // count CRC? TODO
}


WORD TDisk::PreIndexGap() {
  int gap=0;
  switch(nSectors())
  {
  case 9:
    gap=664+6; // 6256 vs 6250
    break;
  case 10:
    gap=50+6+ (60-22);
    break;
  case 11:
    gap=20;
    break;
  }
  return gap;
}


WORD TDisk::RecordLength() {
  return (nSectors()<11)? 614 : 566;
}


BYTE TDisk::SectorGap() {
  return PostDataGap()+PreDataGap();
}


WORD TDisk::TrackGap() {
  return PostIndexGap()+PreIndexGap();
}

#endif//disk

