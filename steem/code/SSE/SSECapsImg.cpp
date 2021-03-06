#include "SSE.h"

#if defined(SSE_DISK_CAPS) // Implementation of CAPS support in Steem SSE

#include "../pch.h"
#include <conditions.h>
#include <cpu.decla.h>
#include <fdc.decla.h>
#include <floppy_drive.decla.h>
#include <iorw.decla.h>
#include <mfp.decla.h>
#include <notifyinit.decla.h>
#include <psg.decla.h>
#include <run.decla.h>

#include "SSECpu.h"
#include "SSEInterrupt.h"
#include "SSEShifter.h"
#include "SSECapsImg.h"
#include "SSEDecla.h"
#include "SSEDebug.h"
#include "SSEFloppy.h"
#include "SSEOption.h"
#include "SSEGhostDisk.h"
#include "SSEVideo.h"


TCaps::TCaps() {
  CAPSIMG_OK=0; // we init in main to keep control of timing/be able to trace
}


TCaps::~TCaps() {
  try { //391 - 64
  if(CAPSIMG_OK)
    CAPSExit();
  }
  catch(...) {
  }
  CAPSIMG_OK=0;
}

#define LOGSECTION LOGSECTION_INIT


int TCaps::Init() {

  Active=FALSE;
  Version=0;
  ContainerID[0]=ContainerID[1]=-1;
  LockedSide[0]=LockedSide[1]=-1;
  LockedTrack[0]=LockedTrack[1]=-1; 
#if defined(SSE_GUI_NOTIFY1)
  SetNotifyInitText(SSE_DISK_CAPS_PLUGIN_FILE);
#endif

  CapsVersionInfo versioninfo;
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
  // this is supposed to catch "SEH" (cpu/system) exceptions
  // Default in VC6, option /EHa in VC2008
  // If not, the DLL must be present
  try {
    CAPSInit();
  }
  catch(...) {
    TRACE_LOG("%s can't be loaded\n",SSE_DISK_CAPS_PLUGIN_FILE);
#if defined(SSE_VAR_REQUIRE_FILES3)
    throw SSE_DISK_CAPS_PLUGIN_FILE;
#endif
    return 0;
  }
  VERIFY( !CAPSGetVersionInfo((void*)&versioninfo,0) );
  ASSERT( !versioninfo.type );
//  TRACE_LOG("Using CapsImg library V%d.%d\n",versioninfo.release,versioninfo.revision);
  TRACE_INIT("CAPSImg.dll loaded, v%d.%d\n",versioninfo.release,versioninfo.revision);
  Version=versioninfo.release*10+versioninfo.revision; 

  CAPSIMG_OK= (Version>0);
  // controller init
  WD1772.type=sizeof(CapsFdc);  // must be >=sizeof(CapsFdc)
  WD1772.model=cfdcmWD1772;
  WD1772.clockfrq=SSE_DISK_CAPS_FREQU; 
  WD1772.drive=SF314; // ain't it cool?
  WD1772.drivecnt=2;
  WD1772.drivemax=0;
  // drives
  SF314[0].type=SF314[1].type=sizeof(CapsDrive); // must be >=sizeof(CapsDrive)
  SF314[0].rpm=SF314[1].rpm=CAPSDRIVE_35DD_RPM;
  SF314[0].maxtrack=SF314[1].maxtrack=CAPSDRIVE_35DD_HST;

  int ec=CAPSFdcInit(&WD1772);
  if(ec!=imgeOk)
  {
    TRACE_LOG("CAPSFdcInit failure %d\n",ec);
    Version=0;
    return 0;
  }

  // the DLL will call them, strange that they're erased at FDC init:
  WD1772.cbdrq=CallbackDRQ;
  WD1772.cbirq=CallbackIRQ;
  WD1772.cbtrk=CallbackTRK;

  // we already create our 2 Caps drives, instead of waiting for an image:
  ContainerID[0]=CAPSAddImage(); //TODO clean up?
  ContainerID[1]=CAPSAddImage();
  ASSERT( ContainerID[0]!=-1 && ContainerID[1]!=-1 );
  WD1772.drivemax=2;
  WD1772.drivecnt=2;

  return Version;
}


void TCaps::Reset() {
    CAPSFdcReset(&WD1772);
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_IMAGE_INFO//SS

int TCaps::InsertDisk(int drive,char* File,CapsImageInfo *img_info) {

  ASSERT( CAPSIMG_OK );
  if(!CAPSIMG_OK)
    return -1;
  ASSERT( !drive || drive==1 );
  ASSERT( img_info );
  ASSERT( ContainerID[drive]!=-1 );
  bool FileIsReadOnly=FloppyDrive[drive].ReadOnly;
  VERIFY( !CAPSLockImage(ContainerID[drive],File) ); // open the CAPS file
  VERIFY( !CAPSGetImageInfo(img_info,ContainerID[drive]) );
  ASSERT( img_info->type==ciitFDD );
  TRACE_LOG("Disk in %c is CAPS release %d rev %d of %d/%d/%d for ",
    drive+'A',img_info->release,img_info->revision,img_info->crdt.day,
    img_info->crdt.month,img_info->crdt.year);
  bool found=0;
  for(int i=0;i<CAPS_MAXPLATFORM;i++)
  {
#ifdef SSE_DEBUG
    if((img_info->platform[i])!=ciipNA)
      TRACE_LOG("%s ",CAPSGetPlatformName(img_info->platform[i]));
#endif
    if(img_info->platform[i]==ciipAtariST 
      || ::SF314[drive].ImageType.Extension==EXT_CTR
      || OPTION_HACKS) //unofficial or multiformat images
      found=true;
  }
  TRACE_LOG("Sides:%d Tracks:%d-%d\n",img_info->maxhead+1,img_info->mincylinder,
    img_info->maxcylinder);
  ASSERT( found );
  if(!found)    // could be a Spectrum disk etc.
  {
    int Ret=FIMAGE_WRONGFORMAT;
    return Ret;
  }
  Active=TRUE;
  SF314[drive].diskattr|=CAPSDRIVE_DA_IN; // indispensable!
  if(!FileIsReadOnly)
    SF314[drive].diskattr&=~CAPSDRIVE_DA_WP; // Sundog
  else
    SF314[drive].diskattr|=CAPSDRIVE_DA_WP; // Jupiter's Master Drive

#if defined(SSE_DRIVE_SINGLE_SIDE_CAPS)
  if(SSEOption.SingleSideDriveMap&(drive+1) && Caps.Version>50)
    SF314[drive].diskattr|=CAPSDRIVE_DA_SS; //tested OK on Dragonflight
#endif
  CAPSFdcInvalidateTrack(&WD1772,drive); // Galaxy Force II
  LockedTrack[drive]=LockedSide[drive]=-1;
  return 0;
}


void TCaps::RemoveDisk(int drive) {
  ASSERT( CAPSIMG_OK );
  if(!CAPSIMG_OK)
    return;
  TRACE_LOG("Drive %c removing image\n",drive+'A');
  VERIFY( !CAPSUnlockImage(Caps.ContainerID[drive]) ); // eject disk
  SF314[drive].diskattr&=~CAPSDRIVE_DA_IN;
  if(::SF314[!drive].ImageType.Manager==MNGR_CAPS)
    Active=FALSE; 
}


void TCaps::WritePsgA(int data) {
  // drive selection 
  if ((data&BIT_1)==0)
    WD1772.drivenew=0;
  else if ((data&BIT_2)==0)
    WD1772.drivenew=1;
  else 
    WD1772.drivenew=-2;
  if(!WD1772.drivenew || WD1772.drivenew==1)
    SF314[WD1772.drivenew].newside=((data&BIT_0)==0);
}


UDWORD TCaps::ReadWD1772(BYTE Line) {

#if defined(SSE_DEBUG)
  Dma.UpdateRegs();
#endif

  UDWORD data=CAPSFdcRead(&WD1772,Line); 
  if(!Line) // read status register
  {
    mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
    WD1772.lineout&=~CAPSFDC_LO_INTRQ; // and in the WD1772
    int drive=floppy_current_drive(); //TODO code duplication
    if(floppy_mediach[drive])
    {
      if(floppy_mediach[drive]/10!=1) 
        data|=FDC_STR_WRITE_PROTECT;
      else
        data&=~FDC_STR_WRITE_PROTECT;
      TRACE_FDC("FDC SR mediach %d WP %x\n",floppy_mediach[drive],data&FDC_STR_WRITE_PROTECT);
    }
  }
  return data;
}


void TCaps::WriteWD1772(BYTE Line,int data) {

  Dma.UpdateRegs();

  if(!Line) // command
  {
#if defined(SSE_DRIVE_MOTOR_ON_IPF)
    if(!(::WD1772.STR&FDC_STR_MOTOR_ON)) // assume no cycle run!
      ::SF314[DRIVE].State.motor=true;
#endif

  }

  CAPSFdcWrite(&WD1772,Line,data); // send to DLL
  // update MFP register according to Caps' lineout; better?
  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,!!!(WD1772.lineout&CAPSFDC_LO_INTRQ));
}


void TCaps::Hbl() {

  // we run cycles at each HBL if there's an IPF file in. Performance OK
#if defined(SSE_SHIFTER)
  CAPSFdcEmulate(&WD1772,Glue.CurrentScanline.Cycles);
#else
  CAPSFdcEmulate(&WD1772,screen_res==2? 224 : 512);
#endif

}

/*  Callback functions. Since they're static, they access object data like
    any external function, using 'Caps.'
*/

#pragma warning (disable: 4100)//unreferenced formal parameter

void TCaps::CallbackDRQ(PCAPSFDC pc, UDWORD setting) {

#if defined(SSE_DEBUG) || !defined(VC_BUILD)//?
  Dma.UpdateRegs();
#endif

#if defined(SSE_DMA_DRQ)
  ::WD1772.DR=Caps.WD1772.r_data;
  Dma.Drq();
  Caps.WD1772.r_data=::WD1772.DR;
#else
  // transfer one byte
  if(!(Dma.MCR&BIT_8)) // disk->RAM
    Dma.AddToFifo( CAPSFdcGetInfo(cfdciR_Data,&Caps.WD1772,0) );
  else // RAM -> disk
    Caps.WD1772.r_data=Dma.GetFifoByte();  
#endif
  
  Caps.WD1772.r_st1&=~CAPSFDC_SR_IP_DRQ; // The Pawn
  Caps.WD1772.lineout&=~CAPSFDC_LO_DRQ;

}


void TCaps::CallbackIRQ(PCAPSFDC pc, UDWORD lineout) {
  ASSERT(pc==&Caps.WD1772);
  // function called to clear IRQ, can mess with sound (Jupiter's Masterdrive)
  if(lineout) {
#if defined(SSE_DEBUG)
    if(TRACE_ENABLED(LOGSECTION_FDC)) 
      Dma.UpdateRegs(true);
    else
#endif
      Dma.UpdateRegs(); // why it only worked in boiler, log on...

#if defined(SSE_DRIVE_SOUND)
    if(OPTION_DRIVE_SOUND)
      ::SF314[DRIVE].Sound_CheckIrq();
#endif
  }

  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,!(lineout&CAPSFDC_LO_INTRQ));
}

#if !defined(SSE_BOILER_TRACE_CONTROL)
#undef LOGSECTION
#define LOGSECTION LOGSECTION_IMAGE_INFO
#endif

void TCaps::CallbackTRK(PCAPSFDC pc, UDWORD drive) {

  int side=Caps.SF314[drive].side;
  int track=Caps.SF314[drive].track;
  CapsTrackInfoT2 track_info; // apparently we must use type 2...
  track_info.type=1;
  UDWORD flags=DI_LOCK_DENALT|DI_LOCK_DENVAR|DI_LOCK_UPDATEFD|DI_LOCK_TYPE;
  
  CapsRevolutionInfo CRI;

  if(Caps.LockedSide[drive]!=side || Caps.LockedTrack[drive]!=track)
    CAPSSetRevolution(Caps.ContainerID[drive],0); // new track, reset #revs just in case

  VERIFY( !CAPSLockTrack((PCAPSTRACKINFO)&track_info,Caps.ContainerID[drive],
    track,side,flags) );

  CAPSGetInfo(&CRI,Caps.ContainerID[drive],track,side,cgiitRevolution,0);
  TRACE_LOG("max rev %d real %d next %d\n",CRI.max,CRI.real,CRI.next);
  ASSERT( track==(int)track_info.cylinder );
  //ASSERT( !track_info.sectorsize );//normally 0
  TRACE_LOG("CAPS Lock %c:S%dT%d flags %X sectors %d bits %d overlap %d startbit %d timebuf %x\n",
    drive+'A',side,track,flags,track_info.sectorcnt,track_info.tracklen,track_info.overlap,track_info.startbit,track_info.timebuf);
  Caps.SF314[drive].trackbuf=track_info.trackbuf;
  Caps.SF314[drive].timebuf=track_info.timebuf;
  Caps.SF314[drive].tracklen = track_info.tracklen;
  Caps.SF314[drive].overlap = track_info.overlap;
  Caps.SF314[drive].ttype=track_info.type;//?
  Caps.LockedSide[drive]=side;
  Caps.LockedTrack[drive]=track;

#if defined(SSE_DEBUG_IPF_TRACE_SECTORS) // debug info
  if(::SF314[drive].ImageType.Extension==EXT_IPF) // not CTR
  {
    CapsSectorInfo CSI;
    DWORD sec_num;
    TRACE_LOG("sector info (encoder,cell type,data,gap info)\n");
    for(sec_num=1;sec_num<=track_info.sectorcnt;sec_num++)
    {
      CAPSGetInfo(&CSI,Caps.ContainerID[drive],track,side,cgiitSector,sec_num-1);
      TRACE_LOG("#%d|%d|%d|%d %d %d|%d %d %d %d %d %d %d\n",
        sec_num,
        CSI.enctype,      // encoder type
        CSI.celltype,     // bitcell type
        CSI.descdatasize, // data size in bits from IPF descriptor
        CSI.datasize,     // data size in bits from decoder
        CSI.datastart,    // data start position in bits from decoder
        CSI.descgapsize,  // gap size in bits from IPF descriptor
        CSI.gapsize,      // gap size in bits from decoder
        CSI.gapstart,     // gap start position in bits from decoder
        CSI.gapsizews0,   // gap size before write splice
        CSI.gapsizews1,   // gap size after write splice
        CSI.gapws0mode,   // gap size mode before write splice
        CSI.gapws1mode);   // gap size mode after write splice
    }
  }
#endif

}

#undef LOGSECTION

#endif//caps
