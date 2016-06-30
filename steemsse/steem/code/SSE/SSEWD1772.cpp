/*  WD1772 is the object in Steem taking care of various aspects
    of emulation of this disk formatter/controller.
    Because there are many disk image types available, and some
    coming with a WD1772 emulator of their own, and because file
    fdc.cpp already contains Steem's original emulation, which is
    kept, this object can be seen as a dispatcher with integrated
    emulator for STW and SCP images, which we call the 'WD1772 emulation'
    as opposed to Steem's 'native' emulation (fdc.cpp), and some
    other emulation variables and facilities.
    The dispatcher also takes care of ghost disks.
*/

#include "SSE.h"

#if defined(STEVEN_SEAGAL) && defined(SSE_FDC)

#include "../pch.h"
#ifdef WIN32
#include <pasti/pasti.h>
#endif
#include <conditions.h>
#include <cpu.decla.h>
#include <fdc.decla.h>
#include <floppy_drive.decla.h>
#include <iorw.decla.h>
#include <mfp.decla.h>
#include <psg.decla.h>
#include <run.decla.h>
#include "SSEDebug.h"
#include "SSEFloppy.h"
#include "SSEInterrupt.h"
#include "SSEWD1772.h"

//#if defined(SSE_FDC)
//#if defined(SSE_WD1772)

#define LOGSECTION LOGSECTION_FDC

#if defined(SSE_WD1772_RESET)

void TWD1772::Reset(bool Cold) {
  STR=2; // funny, like ACIA
#ifdef SSE_FDC_FORCE_INTERRUPT
  InterruptCondition=0;
#endif
#if defined(SSE_FDC_INDEX_PULSE_COUNTER)
 // IndexCounter=0;
#endif

#ifdef SSE_WD1772_REG2_B
  StatusType=1;
#endif

#if defined(SSE_DISK_GHOST)
  Lines.CommandWasIntercepted=0;
#endif

#if defined(SSE_WD1772_PHASE) && SSE_VERSION>=372
  prg_phase=WD_READY;
#endif

}


#endif//reset

/////////////////////////////////// GHOST /////////////////////////////////////

#if defined(SSE_DISK_GHOST)
/*  Ghost disks are described in doc/sse/stg.txt and implemented in
    SSEGhostDisk.
    Here's the part that does the interception.
*/

bool TWD1772::CheckGhostDisk(BYTE drive, BYTE io_src_b) {

  TWD1772IDField IDField;
  IDField.track=SF314[drive].Track(); // not TR
  IDField.side=YM2149.SelectedSide;
  IDField.num=SR; 
  WORD nbytes;
  
#if defined(SSE_DISK_GHOST_SECTOR)
/*  Simplest case: the game writes sectors using the "single sector"
    way. Super Cycles, Sundog, Great Courts.
    We don't have "len" info, we guess.
    The sector counter could be set for multiple command calls (eg 9
    for all sectors), so we only envision 512/1024.
*/
  if((io_src_b&0xF0)==0xA0 || (io_src_b&0xF0)==0x80)
  {
    switch(Dma.Counter) {
    case 0:
      nbytes=0;
    case 2:
      nbytes=1024;
      IDField.len=3;
    default:
      nbytes=512;
      IDField.len=2;
    }//sw
  }
  
  // WRITE 1 SECTOR
  if(nbytes && (io_src_b&0xF0)==0xA0)
  {
    CR=io_src_b; //update this...
    if(SF314[drive].CheckGhostDisk(true))
    {
      // bytes ST memory -> our buffer
      for(int i=0;i<nbytes;i++)
        *(GhostDisk[drive].SectorData+i)=Dma.GetFifoByte();
      GhostDisk[drive].WriteSector(&IDField);
      STR=FDC_STR_MOTOR_ON;
      Lines.CommandWasIntercepted=1;
      agenda_fdc_finished(0);
    }
  }
  
  // READ 1 SECTOR
  if(nbytes && (io_src_b&0xF0)==0x80)
  {   
    // sector is in ghost image?
    if(SF314[drive].CheckGhostDisk(false)
      && GhostDisk[drive].ReadSector(&IDField))
    {
      CR=io_src_b; //update this...
      STR=FDC_STR_MOTOR_ON;
      for(int i=0;i<nbytes;i++)
        Dma.AddToFifo(*(GhostDisk[drive].SectorData+i));
      Lines.CommandWasIntercepted=1;
      agenda_fdc_finished(0); 
    }
  }
#endif//#if defined(SSE_DISK_GHOST_SECTOR)
  
#if defined(SSE_DISK_GHOST_MULTIPLE_SECTORS)
/*  For multiples sectors:
    - We don't IRQ, the program will use D0 (so we hope).
    - We assume sectors are 512 bytes, which is wrong in
    the only case we know yet: Platoon, but it still works
    because what counts is the #bytes transferred between
    disk and RAM.
    We needed to implement multiple sectors support for Platoon,
    but let's not complain, it's cheap, at least we don't need to
    add Format support even though the game uses command Format (with
    1024byte sectors).
*/
  
  if((io_src_b&0xF0)==0xB0 || (io_src_b&0xF0)==0x90)
  {
    IDField.len=2;
    nbytes=IDField.nBytes(); 
  }
  
  // WRITE MULTIPLE SECTORS
  if(nbytes && (io_src_b&0xF0)==0xB0)
  {
    if(SF314[drive].CheckGhostDisk(true))
    {
      CR=io_src_b; //update this...
      // for all sectors
      const int k=Dma.Counter;
      for(int j=0;j<k;j++)
      {
        // bytes ST memory -> our buffer
        for(int i=0;i<nbytes;i++)
        {
          *(GhostDisk[drive].SectorData+i)=Dma.GetFifoByte();
        }//nxt i
        GhostDisk[drive].WriteSector(&IDField); // write 1 sector
        IDField.num=++SR;
      }//nxt j
      STR=FDC_STR_MOTOR_ON; // but no IRQ
      Lines.CommandWasIntercepted=1;
    }
  }//multi-write
 
  // READ MULTIPLE SECTORS
  if(nbytes && (io_src_b&0xF0)==0x90)
  { 
    if(SF314[drive].CheckGhostDisk(false))
    {
      // for all sectors
      const int k=Dma.Counter;
      for(int j=0;j<k;j++)
      {
        // sector is in ghost image?
        if(SF314[drive].State.ghost 
          && GhostDisk[drive].ReadSector(&IDField))
        {
          CR=io_src_b; //update this...
          for(int i=0;i<nbytes;i++)
          {
            Dma.AddToFifo( *(GhostDisk[drive].SectorData+i) );
          } //nxt i
          STR=FDC_STR_MOTOR_ON;
          Lines.CommandWasIntercepted=1;
          IDField.num=++SR; // update both SR and ID field's num
        }
      }//nxt j
    }
  }//multi-read
  
#endif//multiple
  
#if defined(SSE_DISK_GHOST_FAKE_FORMAT)
  // FAKE FORMAT
  if((io_src_b&0xF0)==0xF0)
  {
    STR=STR_MO;//FDC_STR_MOTOR_ON;
    Lines.CommandWasIntercepted=1;
    Dma.BaseAddress+=6250;    
    Dma.Counter-=6250/512;//Platoon
    agenda_fdc_finished(0);// "alright"
  }
#endif
  
  
#if defined(SSE_DISK_GHOST_SECTOR_STX1) && USE_PASTI
/*  Pasti keeps its own variables for all DMA/FDC emulation. This is what
    makes mixing difficult. Updating here is the best way I've found so
    far. TODO
*/
  if(Lines.CommandWasIntercepted && hPasti
    && SF314[drive].ImageType.Manager==MNGR_PASTI)
  {
    ASSERT( !PASTI_JUST_STX || SF314[drive].ImageType.Extension==EXT_STX );
    pasti_update_reg(0xff8609,(Dma.BaseAddress&0xff0000)>>16);
    pasti_update_reg(0xff860b,(Dma.BaseAddress&0xff00)>>8);
    pasti_update_reg(0xff860d,(Dma.BaseAddress&0xff));
  }
#endif//#if defined(SSE_DISK_GHOST_SECTOR_STX1)
  
  return (bool) Lines.CommandWasIntercepted;
  
}

#endif//ghost

////////////////////////////////////// IO /////////////////////////////////////
/*  DmaIO -> WD1772IO
    IO is complicated because there are various WD1772 emulations running,
    we must call the correct one!
*/


BYTE TWD1772::IORead(BYTE Line) {
  
#ifdef SSE_DEBUG
  ASSERT( Line<=3 );
  BYTE drive=DRIVE;
  //drive=floppy_current_drive();
#else
#define drive DRIVE
#endif

  BYTE ior_byte;  

  // Steem handling

  switch(Line){

  case 0: // STR
    
#if defined(SSE_DISK_GHOST)
    if(Lines.CommandWasIntercepted)
    {
      ior_byte=STR; // just "motor on" $80, but need agenda???
    }
    else
#endif
    {
      // IP
      if(floppy_track_index_pulse_active())
        STR|=FDC_STR_T1_INDEX_PULSE;
      else
        STR&=BYTE(~FDC_STR_T1_INDEX_PULSE);
      // WP, SU
#if defined(SSE_WD1772_REG2_B)
      if(StatusType)
#else
      if(floppy_type1_command_active)
#endif
      {
#if SSE_VERSION<=364
        // It was a stupid bug because of Aladin.
        // The Macintosh boot disk mustn't be write protected (Hint).
        STR&=(~FDC_STR_WRITE_PROTECT); 
#endif
        // disk has just been changed (30 VBL set at SetDisk())
        if(floppy_mediach[drive])
        {
#if SSE_VERSION>364
          STR&=(~FDC_STR_WRITE_PROTECT);
#endif
          if(floppy_mediach[drive]/10!=1) 
            STR|=FDC_STR_WRITE_PROTECT;
        }
#if SSE_VERSION>364 
        // Permanent status if disk is in
        else if (FloppyDrive[drive].ReadOnly && !FloppyDrive[drive].Empty())
          STR|=FDC_STR_WRITE_PROTECT;
#endif

#if defined(SSE_DISK_STW) // was set already (hopefully)
        if(SF314[drive].ImageType.Manager==MNGR_WD1772)
          ; else
#endif
          if(fdc_spinning_up)
            STR&=BYTE(~FDC_STR_T1_SPINUP_COMPLETE);
          else
            STR|=FDC_STR_T1_SPINUP_COMPLETE;
      
      } // else it should be set in fdc_execute()
      
      if ((mfp_reg[MFPR_GPIP] & BIT_5)==0) // IRQ is currently raised
      {
        LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
          " - Reading status register as "+Str(itoa(STR,d2_t_buf,2)).LPad(8,'0')+
          " ($"+HEXSl(STR,2)+"), clearing IRQ"); )
          floppy_irq_flag=0;

#if defined(SSE_FDC_FORCE_INTERRUPT_D8)
/*
WD doc:
"When using the immediate interrupt condition (i3 = 1) an interrupt
 is immediately generated and the current command terminated. 
 Reading the status or writing to the Command Register does not
 automatically clear the interrupt. The Hex D0 is the only command 
 that enables the immediate interrupt (Hex D8) to clear on a subsequent 
 load Command Register or Read Status Register operation. 
 Follow a Hex D8 with D0 command."
 
    Some cases show that this is wrong:

    Super Monaco Grand Prix -HTL  D8 is cleared by read STR + new command
    Wipe-Out -RPL                 IRQ isn't cleared by just D8-D0


    So, with D8, for both read STR (here) and write CR, we would have:
    "clear IRQ if no condition", then "clear condition",
*/
        if(!ADAT || InterruptCondition!=8)
          mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
        InterruptCondition=0;
#else//before v3.7
        
#if defined(SSE_FDC_FORCE_INTERRUPT)
        if(InterruptCondition!=8)
#endif
          mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
#endif
      }
#if defined(SSE_DISK_STW)
      Lines.irq=0;
#endif
      
      ior_byte=STR;
    }

#if 0 //debug section, original steem
          {
            fdc_str=debug1;
            int fn=floppy_current_drive();
            if (floppy_track_index_pulse_active()){
              fdc_str|=FDC_STR_T1_INDEX_PULSE;
            }else{
              // If not type 1 command we will get here, it is okay to clear
              // it as this bit is only for the DMA chip for type 2/3.
              fdc_str&=BYTE(~FDC_STR_T1_INDEX_PULSE);
            }
            if (floppy_type1_command_active){
              /* From Jorge Cwik
                The FDC has two different
                type of status. There is a "Type I" status after any Type I command,
                and there is a different "status" after types II & III commands. The
                meaning of some of the status bits is different (this probably you
                already know),  but the updating of these bits is different too.

                In a Type II-III status, the write protect bit is updated from the write
                protect signal only when trying to write to the disk (write sector
                or format track), otherwise is clear. This bit is static, once it was
                updated or cleared, it will never change until a new command is
                issued to the FDC.
              */
              fdc_str&=(~FDC_STR_WRITE_PROTECT);
              if (floppy_mediach[fn]){
                if (floppy_mediach[fn]/10!=1) fdc_str|=FDC_STR_WRITE_PROTECT;
              }else if (FloppyDrive[fn].ReadOnly){
                fdc_str|=FDC_STR_WRITE_PROTECT;
              }
              if (fdc_spinning_up){
                fdc_str&=BYTE(~FDC_STR_T1_SPINUP_COMPLETE);
              }else{
                fdc_str|=FDC_STR_T1_SPINUP_COMPLETE;
              }
            } // else it should be set in fdc_execute()
            if ((mfp_reg[MFPR_GPIP] & BIT_5)==0){
              LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
                          " - Reading status register as "+Str(itoa(fdc_str,d2_t_buf,2)).LPad(8,'0')+
                          " ($"+HEXSl(fdc_str,2)+"), clearing IRQ"); )
              floppy_irq_flag=0;
//              TRACE_FDC("R STR MFP_GPIP_FDC_BIT: %d\n",true);
              mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
            }
//            log_DELETE_SOON(Str("FDC: ")+HEXSl(old_pc,6)+" - reading FDC status register as $"+HEXSl(fdc_str,2));
/*
            LOG_ONLY( if (mode==STEM_MODE_CPU) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
                            " - Read status register as $"+HEXSl(fdc_str,2)); )
*/
            ASSERT(ior_byte==fdc_str);
          }
#endif//debug

#if defined(SSE_BOILER_TRACE_CONTROL)
  if(TRACE_MASK3 & TRACE_CONTROL_FDCSTR)
#endif
  {
#if !defined(SSE_DEBUG_TRACE_IDE) || defined(SSE_DEBUG_FDC_TRACE_STR)
    TRACE_LOG("FDC STR %X PC %X\n",ior_byte,old_pc);
#endif
  }
      break;

    case 1:
      ior_byte=TR;
//      TRACE_LOG("FDC R TR %d\n",ior_byte);
      TRACE_LOG("FDC TR R %d PC %X\n",ior_byte,old_pc);
      break;      
    case 2:
      ior_byte=SR;
//      TRACE_LOG("FDC R SR %d\n",ior_byte);
      TRACE_LOG("FDC SR R %d PC %X\n",ior_byte,old_pc);
      break;
    case 3:
      ior_byte=DR;
//      TRACE_LOG("FDC R DR %d\n",ior_byte);
      TRACE_LOG("FDC DR R %d PC %X\n",ior_byte,old_pc);
      break;
    }//sw

    // IPF handling
#if defined(SSE_IPF)

#if defined(SSE_DISK_IMAGETYPE1)
    if(SF314[drive].ImageType.Manager==MNGR_CAPS)
#else
    if(Caps.IsIpf(drive))
#endif 
      ior_byte=Caps.ReadWD1772(Line);
#endif

  return ior_byte;

#ifndef SSE_DEBUG
#undef drive
#endif
}


void TWD1772::IOWrite(BYTE Line,BYTE io_src_b) {

  ASSERT( Line<=3 );

#ifdef SSE_DEBUG
  BYTE drive=DRIVE;
#else
#define drive DRIVE
#endif

  switch(Line)
  {
  case 0: // CR - could be blocked, can't record here :(
  {

#if defined(SSE_DMA_TRACK_TRANSFER2) // we reset it here so it works for CAPS too
    Dma.Datachunk=0;
#endif

#if defined(SSE_DEBUG) && defined(SSE_DRIVE) && defined(SSE_DISK_IMAGETYPE)//&& SSE_VERSION>=364
#if defined(SSE_BOILER_TRACE_CONTROL)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
#endif
    {
      if( SF314[drive].ImageType.Manager==MNGR_WD1772 
        && (CR&0xF0)==0x90 && !(STR&STR_RNF))
        TRACE_LOG("\n");

      BYTE drive_char= (psg_reg[PSGR_PORT_A]&6)==6? '?' : 'A'+drive;
      TRACE_LOG("%d FDC(%d) CR $%02X %c:%d STR %X TR %d CYL %d SR %d DR %d dma $%X #%d PC $%X\n",
        ACT,SF314[drive].ImageType.Manager,io_src_b,drive_char,CURRENT_SIDE,STR,TR,SF314[drive].Track(),SR,DR,Dma.BaseAddress,Dma.Counter,old_pc);
    }
#endif

#if defined(SSE_DEBUG) && defined(SSE_DRIVE) && defined(SSE_DRIVE_COMPUTE_BOOT_CHECKSUM)
//#undef LOGSECTION
//#define LOGSECTION LOGSECTION_IMAGE_INFO
/*  Used this for Auto239.
    Gives the checksum of bootsector.
    $1234 means executable. Use last WORD to adjust.
*/
    if(SF314[drive].SectorChecksum)
      TRACE_LOG("%c: bootsector checksum $%X\n",'A'+drive,SF314[drive].SectorChecksum);
    SF314[drive].SectorChecksum=0;
//#undef LOGSECTION
//#define LOGSECTION LOGSECTION_FDC
#endif//checksum

#if !defined(SSE_DISK_IMAGETYPE) //old block "can_send"
    bool can_send=true; // are we in Steem's native emu?
#if defined(SSE_IPF)
    can_send=can_send&&!Caps.IsIpf(drive);
#endif
#if USE_PASTI 
    can_send=can_send&&!(hPasti && pasti_active
#if defined(SSE_DRIVE) && defined(SSE_PASTI_ONLY_STX)
      && (!PASTI_JUST_STX || SF314[drive].ImageType==3)
#endif            
        );
#endif//pasti
#endif//SSE_DISK_IMAGETYPE

#if defined(SSE_DISK_GHOST)
/*  For STX, IPF, CTR, we check if we should intercept FDC command
    and use ghost disk.
*/
    Lines.CommandWasIntercepted=0; // reset this at each new whatever command 
    if(SSE_GHOST_DISK)
    {
      if((SF314[drive].ImageType.Manager==MNGR_PASTI 
        && SF314[drive].ImageType.Extension==EXT_STX)
        || SF314[drive].ImageType.Manager==MNGR_CAPS
        || SF314[drive].ImageType.Extension==EXT_SCP)
        CheckGhostDisk(drive,io_src_b); // updates CommandWasIntercepted
    }
#endif//ghost

#if defined(SSE_DRIVE_SOUND)
      if(SSEOption.DriveSound)
#if defined(SSE_DISK_GHOST)
      if(!Lines.CommandWasIntercepted) //would mess registers, and is instant
#endif
      {
#if defined(SSE_DMA)
        Dma.UpdateRegs();
#endif
        SF314[drive].TrackAtCommand=SF314[drive].Track(); //record
#if defined(SSE_DRIVE_SOUND_SINGLE_SET) // drive B uses sounds of A
        SF314[drive].Sound_CheckCommand(io_src_b);
#else
        SF314[0].Sound_CheckCommand(io_src_b);
#endif
      }
#endif//sound

      // Steem's native and WD1772 managers
#if defined(SSE_DISK_IMAGETYPE)
    if(SF314[drive].ImageType.Manager==MNGR_STEEM)
#else
#endif
      floppy_fdc_command(io_src_b); // in fdc.cpp for ST, MSA, DIM, STT
#if defined(SSE_DISK_STW)
    else if(SF314[drive].ImageType.Manager==MNGR_WD1772)
      WriteCR(io_src_b); // for STW, SCP, HFE
#endif
    break;
  }

  case 1: // TR
#if defined(SSE_BOILER_TRACE_CONTROL)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
#endif
    TRACE_LOG("FDC TR W %d PC %X\n",io_src_b,old_pc);

#if defined(SSE_FDC_CHANGE_TRACK_WHILE_BUSY)
    if(!(STR&FDC_STR_BUSY)||ADAT)
      fdc_tr=io_src_b;
#else
    if (!(STR&FDC_STR_BUSY)){
      log_to(LOGSECTION_FDC,EasyStr("FDC: ")+HEXSl(old_pc,6)+" - Setting FDC track register to "+io_src_b);
      TR=io_src_b;
    }else{
      log_to(LOGSECTION_FDC,EasyStr("FDC: ")+HEXSl(old_pc,6)+" - Can't set FDC track register to "+io_src_b+", FDC is busy");
    }
#endif
    break;

  case 2: // SR
#if defined(SSE_BOILER_TRACE_CONTROL)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
#endif
    TRACE_LOG("FDC SR W %d PC %X\n",io_src_b,old_pc);
//    TRACE_LOG("FDC SR %d (pc %x)\n",io_src_b,old_pc );

#if defined(SSE_FDC_CHANGE_SECTOR_WHILE_BUSY)
    if(!(STR&FDC_STR_BUSY)||ADAT)
      SR=io_src_b; // fixes Delirious 4 loader without Pasti
    
#else
    if (!(STR & FDC_STR_BUSY)){
      log_to(LOGSECTION_FDC,EasyStr("FDC: ")+HEXSl(old_pc,6)+" - Setting FDC sector register to "+io_src_b);
      SR=io_src_b;
    }else{
      log_to(LOGSECTION_FDC,EasyStr("FDC: ")+HEXSl(old_pc,6)+" - Can't set FDC sector register to "+io_src_b+", FDC is busy");
    }
#endif
    break;

  case 3: // DR
#if defined(SSE_BOILER_TRACE_CONTROL)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
#endif
//    TRACE_LOG("FDC DR %d\n",io_src_b);
    TRACE_LOG("FDC DR W %d PC %X\n",io_src_b,old_pc);

    log_to(LOGSECTION_FDC,EasyStr("FDC: ")+HEXSl(old_pc,6)+" - Setting FDC data register to "+io_src_b);
    DR=io_src_b;
    break;
  }
  
  // IPF handling
#if defined(SSE_IPF)
#if defined(SSE_DISK_IMAGETYPE1)
  if(SF314[drive].ImageType.Manager==MNGR_CAPS)
#else
  if(Caps.IsIpf(drive))
#endif 
#if defined(SSE_DISK_GHOST)
    if(SSE_GHOST_DISK && Lines.CommandWasIntercepted)
    {
      TRACE_LOG("Caps doesn't get command %x\n",io_src_b);
      Caps.WD1772.r_command=CR;//update this...
    }
    else
#endif
      Caps.WriteWD1772(Line,io_src_b);
#endif//IPF
      
#ifndef SSE_DEBUG
#undef drive
#endif
  
}

///////////////////////////////////// ACC /////////////////////////////////////

BYTE TWD1772::CommandType(int command) {
  // return type 1-4 of this FDC command
  if(command==-1) //default: current command
    command=CR;
  BYTE type;
  if(!(command&BIT_7))
    type=1;
  else if(!(command&BIT_6))
    type=2;
  else if((command&0xF0)==0xD0) //1101
    type=4;
  else
    type=3;
  return type;
}


#if defined(SSE_DEBUG) || defined(SSE_OSD_DRIVE_LED)

int TWD1772::WritingToDisk() { // could do this at DMA level?
  return((CR&0xF0)==0xF0 || (CR&0xE0)==0xA0 || (CR&0xE0)==0xB0) ;
}

#endif

/////////#endif//SSE_FDC


///////////////////////////////// Debug ///////////////////////////////////////

#if defined(SSE_DEBUG)

char* wd_phase_name[]={  // Coooool! note change if change enum !!!!!
    "WD_READY",
    "WD_TYPEI_SPINUP",
    "WD_TYPEI_SPUNUP", // spunup must be right after spinup
    "WD_TYPEI_SEEK",
    "WD_TYPEI_STEP_UPDATE",
    "WD_TYPEI_STEP",
    "WD_TYPEI_STEP_PULSE",
    "WD_TYPEI_CHECK_VERIFY",
    "WD_TYPEI_HEAD_SETTLE",
    "WD_TYPEI_FIND_ID",
    "WD_TYPEI_READ_ID", // read ID must be right after find ID
    "WD_TYPEI_TEST_ID", // test ID must be right after read ID
    "WD_TYPEII_SPINUP",
    "WD_TYPEII_SPUNUP", // spunup must be right after spinup
    "WD_TYPEII_HEAD_SETTLE", //head settle must be right after spunup
    "WD_TYPEII_FIND_ID",
    "WD_TYPEII_READ_ID", // read ID must be right after find ID
    "WD_TYPEII_TEST_ID", // test ID must be right after read ID
    "WD_TYPEII_FIND_DAM",
    "WD_TYPEII_READ_DATA",
    "WD_TYPEII_READ_CRC",
    "WD_TYPEII_CHECK_MULTIPLE",
    "WD_TYPEII_WRITE_DAM",
    "WD_TYPEII_WRITE_DATA",
    "WD_TYPEII_WRITE_CRC",
    "WD_TYPEIII_SPINUP",
    "WD_TYPEIII_SPUNUP", // spunup must be right after spinup
    "WD_TYPEIII_HEAD_SETTLE", //head settle must be right after spunup
    "WD_TYPEIII_IP_START", // start read/write
    "WD_TYPEIII_FIND_ID",
    "WD_TYPEIII_READ_ID", // read ID must be right after find ID
    "WD_TYPEIII_TEST_ID",
    "WD_TYPEIII_READ_DATA",
    "WD_TYPEIII_WRITE_DATA",
    "WD_TYPEIII_WRITE_DATA2", // CRC is 1 byte in RAM -> 2 bytes on disk
    "WD_TYPEIV_4", // $D4
    "WD_TYPEIV_8", // $D8
    "WD_MOTOR_OFF",
  };

/*
Bits:
Bit 7 - Motor On.  This bit is high when the drive motor is on, and
low when the motor is off.

Bit 6 - Write Protect.  This bit is not used during reads.  During
writes, this bit is high when the disk is write protected.

Bit 5 - Spin-up / Record Type.  For Type I commands, this bit is low
during the 6-revolution motor spin-up time.  This bit is high after
spin-up.  For Type II and Type III commands, Bit 5 low indicates a
normal data mark.  Bit 5 high indicates a deleted data mark.

Bit 4 - Record Not Found.  This bit is set if the 177x cannot find the
track, sector, or side which the CPU requested.  Otherwise, this bit
is clear.

Bit 3 - CRC Error.  This bit is high if a sector CRC on disk does not
match the CRC which the 177x computed from the data.  The CRC
polynomial is x^16+x^12+x^5+1.  If the stored CRC matches the newly
calculated CRC, the CRC Error bit is low.  If this bit and the Record
Not Found bit are set, the error was in an ID field.  If this bit is
set but Record Not Found is clear, the error was in a data field.

Bit 2 - Track Zero / Lost Data.  After Type I commands, this bit is 0
if the mechanism is at track zero.  This bit is 1 if the head is not
at track zero.  After Type II or III commands, this bit is 1 if the
CPU did not respond to Data Request (Status bit 1) in time for the
177x to maintain a continuous data flow.  This bit is 0 if the CPU
responded promptly to Data Request.

Bit 1 - Index / Data Request.  On Type I commands, this bit is high
during the index pulse that occurs once per disk rotation.  This bit
is low at all times other than the index pulse.  For Type II and III
commands, Bit 1 high signals the CPU to handle the data register in
order to maintain a continuous flow of data.  Bit 1 is high when the
data register is full during a read or when the data register is empty
during a write.  "Worst case service time" for Data Request is 23.5
cycles.

Bit 0 - Busy.  This bit is 1 when the 177x is busy.  This bit is 0
when the 177x is free for CPU commands.
*/


void TWD1772::TraceStatus() {
  // this embarrassing part proves my ignorance!
  int type=CommandType(fdc_cr);
  TRACE_LOG("( ");
  if(STR&0x80)
    TRACE_LOG("MO ");//Motor On
  if(STR&0x40)
    TRACE_LOG("WP ");// write protect
  if(STR&0x20)
    if(type==1)
      TRACE_LOG("SU "); // Spin-up (meaning up to speed)
    else
      TRACE_LOG("RT "); //Record Type (1=deleted data)
  if(STR&0x10)
    if(type==1)
      TRACE_LOG("SE ");//Seek Error
    else
      TRACE_LOG("RNF ");//Record Not Found
  if(STR&0x08)
    TRACE_LOG("CRC "); //CRC Error
  if(STR&0x04)
    if(type==1) 
      TRACE_LOG("T0 "); // track zero
    else
      TRACE_LOG("LD ");//Lost Data, normally impossible on ST
  if(STR&0x02)
    if(type==1 || type==4) 
      TRACE_LOG("IP "); // index
    else
      TRACE_LOG("DRQ "); // data request
  if(STR&0x01)
    TRACE_LOG("BSY "); // busy
    TRACE_LOG(") "); 
}

#endif

///////////////////////////////// ID Field ////////////////////////////////////

#if defined(SSE_WD1772_IDFIELD)
//#if defined(SSE_DISK_STW) || defined(SSE_DISK_GHOST)

TWD1772IDField::TWD1772IDField() {
#ifdef SSE_UNIX
  memset(this,0,sizeof(TWD1772IDField));
#else
  ZeroMemory(this,sizeof(TWD1772IDField));
#endif
}


WORD TWD1772IDField::nBytes() {
  WORD nbytes=1<<( (len&3) +7); // other way: harder
  return nbytes;
}


void TWD1772IDField::Trace() {
#ifdef SSE_DEBUG
//  TRACE_LOG("ID track %d side %d num %d len %d CRC %X%X\n",track,side,num,len,CRC[0],CRC[1]);
//  TRACE_LOG("ID T%dS%dN%dL%d\n",track,side,num,len);//shorter, no CRC info
  TRACE_LOG("ID T%d S%d N%d L%d CRC%X%X\n",track,side,num,len,CRC[0],CRC[1]);
#endif
}

#endif


/////////////////////////////////// MFM ///////////////////////////////////////
/*
    Correct field must be filled in before calling a function:
    data -> Encode() -> clock and encoded word available 
    encoded word -> Decode() -> data & clock available 
    If mode is FORMAT_CLOCK, the clock byte will have a missing bit
    for bytes $A1 and $C2.
    MFM encoding is partly for fun, we could just store the clock
    and data bytes separately, it would be more efficient.
*/

//#if defined(SSE_DISK_STW)
#if defined(SSE_WD1772_MFM)


#ifdef SSE_WD1772_MFM_PRODUCE_TABLE // one-shot switch...
  // todo, also in bits
 for(int i=0;i<256;i++)
 {
   WD1772.Mfm.data=i;
   WD1772.Mfm.Encode();
   TRACE("D %02X -> C %02X MFM %04X\n",i,WD1772.Mfm.clock,WD1772.Mfm.encoded);
 }
#endif


void TWD1772MFM::Decode() {

#if defined(SSE_DISK_STW_MFM) // not optimised
  WORD encoded_shift=encoded;
  data=clock=0; //BYTEs
  for(int i=0;i<8;i++)
  {
    clock|= (encoded_shift&0x8000) != 0;
    if(i<7)
      clock<<=1;
    encoded_shift<<=1;
    data|= (encoded_shift&0x8000) != 0;
    if(i<7)
      data<<=1,encoded_shift<<=1;
  }
#else
  data=(BYTE)encoded&0xFF;
  clock=(BYTE) ((encoded>>8)&0xFF);
#endif

}


void TWD1772MFM::Encode(int mode) {

  // 1. compute the clock
  clock=0;
  BYTE previous=data_last_bit;
  BYTE current;
  data_last_bit=data&1;
  BYTE data_shift=data;
  for(int i=0;i<8;i++)
  {
    current=data_shift&0x80;
    if(!previous && !current)
      clock|=1;
    if(i<7)
      clock<<=1;
    data_shift<<=1;
    previous=current;
  }
  if(mode==FORMAT_CLOCK)
    if(data==0xA1) // -> $4489
      clock&=~4; // missing bit 2 of clock -> bit 5 of encoded word
    else if(data==0xC2) // -> $5224
      clock&=~2; // missing bit 1 of clock -> bit 4 of encoded word


#if defined(SSE_DISK_STW_MFM)
  // 2. mix clock & data to create a word
  data_shift=data;
  BYTE clock_shift=clock;
  encoded=0;
  for(int i=0;i<8;i++)
  {
    encoded|=(clock_shift&0x80) != 0;
    encoded<<=1; clock_shift<<=1;
    encoded|=(data_shift&0x80) != 0;
    if(i<7)
      encoded<<=1; data_shift<<=1;
  }   
#else//no mfm:
  encoded=clock;
  encoded<<=8;
  encoded|=data;  
#endif
}

#endif

/////////////////////////////////// CRC ///////////////////////////////////////
/*  CRC logic.
    Some insights were gained thanks to the game Dragonflight, that 
    uses CRC tricks.
    An incorrect CRC is part of many protections, so it may be normal.
    TODO: test on hardware
*/

//#if defined(SSE_DISK_STW)
#if defined(SSE_WD1772_CRC)

void TWD1772Crc::Add(BYTE data) {
  fdc_add_to_crc(crc,data); // we just call Steem's original function for now
}


bool TWD1772Crc::Check(TWD1772IDField *IDField) {
  bool ok=IDField->CRC[0]==HIBYTE(crc) && IDField->CRC[1]==LOBYTE(crc);
#ifdef SSE_DEBUG
  if(!ok)
    TRACE_LOG("CRC error - computed: %X - read: %X%X\n",crc,IDField->CRC[0],IDField->CRC[1]);
#endif
  return ok;
}


/*  Contrary to what the doc states, the CRC Register isn't preset to ones 
    ($FFFF) prior to data being shifted through the circuit, but to $CDB4.
    This happens for each $A1 address mark (read or written), so the register
    value after $A1 is the same no matter how many address marks.
    When formatting the backup disk, Dragonflight writes a single $F5 (->$A1)
    in its custom track headers and expects to read value $CDB4.
*/

void TWD1772Crc::Reset() {
  crc=0xCDB4;
}

#endif

/////////////////////////////////// DPLL //////////////////////////////////////
/*  This is the correct algorithm for the WD1772 DPLL (digital phase-locked 
    loop) system, as described in patent US 4808884 A.
    It allows us to read low-level (flux level) disk images such as SCP.
    Thx to Olivier Galibert for some inspiration, otherwise the code
    would be a real MESS, err, mess...
*/

#if defined(SSE_WD1772_DPLL)

int TWD1772Dpll::GetNextBit(int &tm, BYTE drive) {
  ASSERT(drive<=1);
  ASSERT(SF314[drive].ImageType.Extension==EXT_SCP);

  int aa=0;

  BYTE timing_in_us;

  while(ctime-latest_transition>=0)
  {
    aa=ImageSCP[drive].GetNextTransition(timing_in_us); // cycles to next 1
    TRACE_MFM("(%d)",timing_in_us);

#if defined(SSE_WD1772_WEAK_BITS) //NO
/*  We make sure that Dungeon Master's weak bits are paired.
    See sector 0-00-7
    Formed bytes should be random, out of those 2 values only:
      $68 01101000 MFM 944A 1001010001001010 
      $E8 11101000 MFM 544A 0101010001001010 
    With other values, the 1st gate won't even open for your party.
    We nudge by only 1 cycle, but it makes the difference.
    undef 3.7.2
*/
    if(!SSE_HACKS_ON) 
      ; // it is a dangerous hack, protected by the option
    else if(weak_bit_pairing) // also if current timing is even
    {
      aa-=weak_bit_pairing; // 
      TRACE_MFM(" WB%d ",-weak_bit_pairing);
      weak_bit_pairing=0;
    }
    else if(timing_in_us>4 && timing_in_us<8 && (timing_in_us&1))
    { // not exact science...
      if(phase_add-phase_sub && (rand()&1))
        weak_bit_pairing=(phase_add-phase_sub<0) ? 1 : -1;
      else
        weak_bit_pairing=(rand()%3)-1; 
      ASSERT(weak_bit_pairing==-1 || weak_bit_pairing==1 || weak_bit_pairing==0);
      aa+=weak_bit_pairing;
      TRACE_MFM(" WB%d ",weak_bit_pairing);
    }
    else
      weak_bit_pairing=0;
#endif
    latest_transition+=aa;
  }
  int when=latest_transition;

  ASSERT(!(when==-1 || when-ctime<0));

  for(;;) {
    int etime = ctime+delays[slot];

    if(transition_time == 0xffff && etime-when >= 0)
      transition_time = counter;

    if(slot < 8) { //SS I don't understand this, why only <8?
      BYTE mask = 1 << slot;
      if(phase_add & mask)
        counter += 226;
      else if(phase_sub & mask)
        counter += 30;
      else
        counter += increment;

      if((freq_add & mask) && increment < 140)
        increment++;
      else if((freq_sub & mask) && increment > 117)
        increment--;
    } else
      counter += increment;

    slot++;
    tm = etime;
    //ASSERT( 128*16==0x800 );
    if(counter & 0x800)
      break;
  }

  int bit = transition_time != 0xffff;
  
  //if(transition_time != 0xffff) {
  if(bit) { //SS refactoring!
    static const BYTE pha[8] = { 0xf, 0x7, 0x3, 0x1, 0, 0, 0, 0 };
    static const BYTE phs[8] = { 0, 0, 0, 0, 0x1, 0x3, 0x7, 0xf };
    static const BYTE freqa[4][8] = {
      { 0xf, 0x7, 0x3, 0x1, 0, 0, 0, 0 },
      { 0x7, 0x3, 0x1, 0, 0, 0, 0, 0 },
      { 0x7, 0x3, 0x1, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    static const BYTE freqs[4][8] = {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0x1, 0x3, 0x7 },
      { 0, 0, 0, 0, 0, 0x1, 0x3, 0x7 },
      { 0, 0, 0, 0, 0x1, 0x3, 0x7, 0xf },
    };

    int cslot = transition_time >> 8;
    ASSERT( cslot<8 );
    phase_add = pha[cslot];
    phase_sub = phs[cslot];
    int way = transition_time & 0x400 ? 1 : 0;
    if(history & 0x80)
      history = way ? 0x80 : 0x83;
    else if(history & 0x40)
      history = way ? history & 2 : (history & 2) | 1;
    freq_add = freqa[history & 3][cslot];
    freq_sub = freqs[history & 3][cslot];
    history = way ? (history >> 1) | 2 : history >> 1;
  } else
    phase_add = phase_sub = freq_add = freq_sub = 0; //SS don't understand this either

  counter &= 0x7ff;
  ctime = tm;
  transition_time = 0xffff;
  slot = 0;
  ASSERT( bit==0 || bit==1 );
  return bit;
}


void TWD1772Dpll::Reset(int when) {
  counter = 0;
  increment = 128;
  transition_time = 0xffff;
  history = 0x80;
  slot = 0;
  latest_transition= ctime = when;
  phase_add = 0x00;
  phase_sub = 0x00;
  freq_add  = 0x00;
  freq_sub  = 0x00;
  write_position = 0;
  write_start_time = -1;
  SetClock(1); // clock WD1772 = clock CPU, 16 cycles = 2 microseconds
}


void TWD1772Dpll::SetClock(const int &period)
{
  for(int i=0; i<42; i++)
    delays[i] = period*(i+1);
}

#endif

///////////////////////////////////// AM //////////////////////////////////////
// 3rd party-inspired; see note for function WD1772.ShiftBit()

#if defined(SSE_WD1772_AM_LOGIC)

// reset am detector; read returns only on AM detected or clocks elapsed
void TWD1772AmDetector::Enable() {
  Enabled=true;
  nA1=0;
  aminfo|=CAPSFDC_AI_AMDETENABLE|CAPSFDC_AI_CRCENABLE;
  aminfo&=~(CAPSFDC_AI_CRCACTIVE|CAPSFDC_AI_AMACTIVE);
  amisigmask=CAPSFDC_AI_DSRAM;
}

void TWD1772AmDetector::Reset() {
  amdatadelay=2;
  ammarkdist=ammarktype=amdataskip=0; 
  amdecode=aminfo=amisigmask=dsr=0; // dword
  dsrcnt=0; // int
  Enable();
}

#endif

///////////////////////////////// WD1772 emu //////////////////////////////////

/*  This is yet another WD1772 emulation, specially written to handle
    STW disk images. 
    A goal from the start was to be able to use it for another format
    like SCP as well, hence we work with a spinning drive and a flow (of
    bytes).
    In v3.7.2, this emu is also used for HFE disk image support too, so
    this "rewrite" proved much useful.
    We follow Western Digital flow charts, with some additions and (gasp!) 
    corrections. 
    Because we use Dma.Drq() for each byte, the agenda system is too gross.
    Each time an operation takes time, we set up an event that sends here.
    Otherwise we use recursion to hop to next phase, it's better than
    goto.
    So it doesn't work with emulation cycles like CapsImg but with timed
    events like Pasti. Still not sure what's the best approach here.
    For a better support of SCP format, we integrated a "data separator"
    inspired by their competitor SPS/CAPS/Kryoflux, as well as a "DPLL"
    inspired by MESS.
*/


#if defined(SSE_DISK_STW) || defined(SSE_DISK_SCP) || defined(SSE_DISK_HFE)

bool TWD1772::Drq(bool state) {
  Lines.drq=state;
  if(state)
    STR|=STR_DRQ;
  else
    STR&=~STR_DRQ;
  if(state) //check old state?
    return Dma.Drq();
  return state;
}

void TWD1772::Irq(bool state) {

#if defined(SSE_WD1772_AM_LOGIC)
  Amd.Reset();
  Amd.Enabled=false;//??
#endif

  if(state && !Lines.irq)// && (STR&STR_BUSY) ) // so not on "force interrupt"
  {
    IndexCounter=10;
    //TRACE_LOG("%d IP for motor off\n",IndexCounter);
    prg_phase=WD_MOTOR_OFF; // will be changed for $D4
    STR&=~STR_BUSY;
    if(CommandType()==2 || CommandType()==3)
      STR&=~STR_DRQ;

#if defined(SSE_DRIVE_SOUND) && (!defined(SSE_DRIVE_SOUND_SEEK2)\
  ||defined(SSE_DRIVE_SOUND_SEEK5))
    if(SSEOption.DriveSound
#if defined(SSE_DRIVE_SOUND_SEEK5)
      && DRIVE_SOUND_SEEK_SAMPLE
#endif
      )
#if defined(SSE_DRIVE_SOUND_SINGLE_SET) // drive B uses sounds of A
      SF314[DRIVE].Sound_CheckIrq();
#else
      SF314[0].Sound_CheckIrq();
#endif
#endif
    Dma.UpdateRegs(true);
  }
  Lines.irq=state;

  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,!state); // MFP GPIP is "active low"

  // reset drive R/W flags
  SF314[DRIVE].State.reading=SF314[DRIVE].State.writing=0;

}


void TWD1772::Motor(bool state) {
  Lines.motor=state;
  if(state)
    STR|=STR_MO;
  else
    STR&=~STR_MO;
  // only on currently selected drive, if any:
  if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE)
//    SF314[YM2149.SelectedDrive].Motor(state); 
    SF314[DRIVE].Motor(state); 
  else TRACE_LOG("WD motor %d: no drive\n",state);
}


int TWD1772::MsToCycles(int ms) {
#if defined(SSE_INT_MFP_RATIO) 
  return ms*CpuNormalHz/1000; // *8000
#else
  return ms*8000;
#endif
}


#if defined(SSE_WD1772_PHASE)


/*  Here there's no discussion: WD1772 got a new command and it will
    start executing it.
*/

void TWD1772::NewCommand(BYTE command) {
  ASSERT( IMAGE_STW || IMAGE_SCP || IMAGE_HFE);
  //TRACE_LOG("STW new command %X\n",command);
  CR=command;


#if defined(SSE_FDC_FORCE_INTERRUPT_D8)
#else
  if(InterruptCondition==4) // what if 4+8?
    InterruptCondition=0;
#endif

  // reset drive R/W flags
  SF314[DRIVE].State.reading=SF314[DRIVE].State.writing=0;

  BYTE type=CommandType(command);
  
  switch(type)
  {
  case 1: // I
    // Set Busy, Reset CRC, Seek error, DRQ, INTRQ
    STR|=STR_BUSY;
    STR&=~(STR_CRC|STR_SE |STR_WP);
    Drq(0); // this takes care of status bit
    if(InterruptCondition!=8)
      Irq(0);
    InterruptCondition=0;
#if defined(SSE_WD1772_REG2_B)
    StatusType=1;
#else
    floppy_type1_command_active=1; //or 2?
#endif

    // should we wait for spinup (H=0)?
    if(!(CR&CR_H) && !Lines.motor)
    {
      // Set MO wait 6 index pulses
      Motor(true); // this will create the event at each IP until motor off
      IndexCounter=6;
      //TRACE_LOG("%d IP to spin up\n",IndexCounter);
      prg_phase=WD_TYPEI_SPINUP;
    }
    else
    {
      // Doc doesn't state motor is started if it wasn't spinning yet and h=1
      // We assume.
      Motor(true); 
      prg_phase=WD_TYPEI_SPUNUP;
      STR|=STR_SU; // eg ST NICCC 2
      OnUpdate(); //go direct, no delay
    }
    break;

  case 2: // II
    // Set Busy, Reset CRC, DRQ, LD, RNF, WP, Record Type
    STR|=STR_BUSY;
    STR&=~(STR_LD|STR_RNF|STR_WP|STR_RT);
    Drq(0);
    if(InterruptCondition!=8)
      Irq(0);
    InterruptCondition=0;
#if defined(SSE_WD1772_REG2_B)
    StatusType=0;
#else
    floppy_type1_command_active=0;
#endif
    if(!(CR&CR_H) && !Lines.motor)
    {
      Motor(true); 
      IndexCounter=6;
      //TRACE_LOG("%d IP to spin up\n",IndexCounter);
      prg_phase=WD_TYPEII_SPINUP;
    }
    else
    {
      Motor(true); 
      prg_phase=WD_TYPEII_SPUNUP;
      OnUpdate();
    }
    break;

  case 3: // III
    STR|=STR_BUSY;
    STR&=~(STR_LD|STR_RNF|STR_WP|STR_RT); // we add WP
    Drq(0);
    // we add this:
    if(InterruptCondition!=8)
      Irq(0);
    InterruptCondition=0;
#if defined(SSE_WD1772_REG2_B)
    StatusType=0;
#else
    floppy_type1_command_active=0;
#endif

    // we treat the motor / H business as for type II, not as on flow chart
    if(!(CR&CR_H) && !Lines.motor)
    {
      Motor(true); 
      IndexCounter=6;
      //TRACE_LOG("%d IP to spin up\n",IndexCounter);
      prg_phase=WD_TYPEIII_SPINUP;
    }
    else
    {
      Motor(true); 
      prg_phase=WD_TYPEIII_SPUNUP;
      OnUpdate();
    }
    break;

  case 4: // IV
    Motor(true); // also type IV triggers motor
    if(STR&STR_BUSY)
      STR&=~STR_BUSY;
    else // read STR is type I if FDC wasn't busy when interrupted (doc)
#if defined(SSE_WD1772_REG2_B)
      StatusType=1;
#else
      floppy_type1_command_active=2; // something like that?
#endif

    if(CR&CR_I3) // immediate, D8
    {
      InterruptCondition=8;
      Irq(true); 
      prg_phase=WD_MOTOR_OFF;
      IndexCounter=10;
      //TRACE_LOG("%d IP for motor off\n",IndexCounter);
    }
    else if(CR&CR_I2) // each IP, D4
    {
      prg_phase=WD_TYPEIV_4;
      InterruptCondition=4;
      IndexCounter=1;
    }
    else // D0, just stop motor in 9 rev
    {
      // no IRQ!
#if defined(SSE_FDC_FORCE_INTERRUPT) 
      if(InterruptCondition!=8)
        Irq(false); // but could have to clear it (Wipe-Out)
#endif
      prg_phase=WD_MOTOR_OFF;
      IndexCounter=10;
      InterruptCondition=0;
      //TRACE_LOG("%d IP for motor off\n",IndexCounter);
    }
    break;
  }//sw

}

#endif


#if defined(SSE_DRIVE_INDEX_PULSE)
/*  Drive calls this function at IP if it's selected.
    Whether the WD1772 is waiting for it or not.
*/
#if defined(SSE_DRIVE_INDEX_PULSE2)
void TWD1772::OnIndexPulse(int id,bool image_triggered) {
#else
void TWD1772::OnIndexPulse(int id) {
#endif
  IndexCounter--; // We set counter then decrement until 0

#ifdef SSE_DEBUG
  if(prg_phase!=WD_READY)
#if defined(SSE_DRIVE_INDEX_PULSE2)
  TRACE_LOG("%c: IP #%d (%s) (%s) CR %X TR %d SR %d DR %d STR %X ACT %d\n",
    'A'+id,IndexCounter,wd_phase_name[prg_phase],image_triggered?"triggered":"event",
    CR,TR,SR,DR,STR,ACT);
#else
  TRACE_LOG("%c: IP #%d (%s) CR %X TR %d SR %d DR %d\n",
    'A'+id,IndexCounter,wd_phase_name[prg_phase], CR,TR,SR,DR);
#endif
#endif

  if(!IndexCounter)
  {
    switch(prg_phase)
    {
    case WD_TYPEI_SPINUP:
      STR|=STR_SU;
    case WD_TYPEII_SPINUP:
    case WD_TYPEIII_SPINUP:
      prg_phase++; // we assume next phase is spunup for this optimisation
      OnUpdate();
      break;

    case WD_TYPEI_FIND_ID:
    case WD_TYPEI_READ_ID:
    case WD_TYPEII_FIND_ID:
    case WD_TYPEII_READ_ID:
#if defined(SSE_WD1772_371) // undocumented but I knew it, v3.7.1
    case WD_TYPEIII_FIND_ID: // Antago SCP (note: disk must be read-only)
#endif
    case WD_TYPEIII_READ_ID:
      TRACE_LOG("Find ID timeout\n");
      STR|=STR_SE; // RNF is same bit as SE, OSD will be set by Dma.UpdateRegs
      Irq(true);      
      break;

    case WD_TYPEIII_IP_START: 
      IndexCounter=1;
      n_format_bytes=0;
      //TRACE_LOG("%d IP for stop track operation\n",IndexCounter);
      if(CR&CR_TYPEIII_WRITE)
      {
        TRACE_LOG("Format track %c:S%d T%d (DMA %d sectors)\n",'A'+DRIVE,CURRENT_SIDE,CURRENT_TRACK,Dma.Counter);
        prg_phase=WD_TYPEIII_WRITE_DATA;
#if defined(SSE_WD1772_F7_ESCAPE)
        F7_escaping=false;
#endif
        OnUpdate(); // hop
      }
      else
      {
        //TRACE_LOG("Start reading track\n");
        TRACE_LOG("Read track %c:S%d F%d  (DMA %d sectors)\n",'A'+DRIVE,CURRENT_SIDE,CURRENT_TRACK,Dma.Counter);
        prg_phase=WD_TYPEIII_READ_DATA;
#if defined(SSE_WD1772_AM_LOGIC)
        Amd.Reset();
#endif
        Read();
      }
      break;

    case WD_TYPEIII_WRITE_DATA:
    case WD_TYPEIII_WRITE_DATA2: //! 
    case WD_TYPEIII_READ_DATA:
      // stop R/W, but not for type II: Realm of the Trolls
      SF314[DRIVE].State.reading=SF314[DRIVE].State.writing=0; 
#ifdef SSE_DEBUG
      if(WritingToDisk()) TRACE_LOG("%d bytes transferred\n",Dma.Datachunk*16);
      TRACE_LOG("\n");//stop list of sector nums
#endif
      Irq(true);
      break;

    case WD_TYPEIV_4: // $D4: raise IRQ at each IP until new command
      ASSERT( !Lines.irq );
      Irq(true);
      //ASSERT( prg_phase==WD_TYPEIV_4 );
      prg_phase=WD_TYPEIV_4;
      IndexCounter=1;
      TRACE_LOG("%d IP for $D4 interrupt\n",IndexCounter);
      break;

    case WD_MOTOR_OFF:
      Motor(false);
#if defined(SSE_DISK_GHOST)
      Lines.CommandWasIntercepted=0;
#endif
#if defined(SSE_WD1772_371)
      prg_phase=WD_READY;
#endif
      break;

    default: // drive is spinning, WD isn't counting
#if defined(SSE_DRIVE_INDEX_PULSE2)
      if(!image_triggered)
#endif
        OnUpdate();//just in case... ???
      break; 
    }//sw
  
  }//if
  else
  {
    n_format_bytes=n00=nFF=0; //?
#if defined(SSE_DRIVE_INDEX_PULSE2)
    if(!image_triggered)
#endif
      OnUpdate(); // to trigger Read() or Write() if needed: Delirious 3
  }

//  TRACE_LOG("WD1772 IP %d byte %d\n",IndexCounter,Disk[DRIVE].current_byte);
}

#endif


#if defined(SSE_FLOPPY_EVENT)

void TWD1772::OnUpdate() {

#if defined(SSE_FLOPPY_EVENT2)
  update_time=time_of_next_event+n_cpu_cycles_per_second; // we come here anyway
#else
  //TRACE_LOG("wd1772 event cpu %d\n",ACT);
  update_time=ACT+n_cpu_cycles_per_second; // we come here anyway
#endif

  if(!(IMAGE_STW)&&!(IMAGE_SCP)&&!(IMAGE_HFE)) // only for those images
  {
    return; 
  }
  switch(prg_phase)
  {
    
  case WD_TYPEI_SPUNUP:
    // we come here after 6 IP or directly
    if(CR&CR_STEP) // is command a step, step-in, step-out?
    {
      // if step-in or step-out, update DIRC line
      if((CR&CR_STEP)==CR_STEP_IN) 
        Lines.direction=true;
      else if((CR&CR_STEP)==CR_STEP_OUT) 
        Lines.direction=false; 
      // goto B or C according to flag u
      prg_phase=(CR&CR_U)?WD_TYPEI_STEP_UPDATE:WD_TYPEI_STEP;
      OnUpdate();
    }
    else  // else it's seek/restore
    {
      prg_phase=WD_TYPEI_SEEK; // goto 'A'
      if((CR&CR_SEEK)==CR_RESTORE) // restore?
      {
        TR=0xFF;
        Lines.track0=(floppy_head_track[DRIVE]==0);
        if(Lines.track0)
          TR=0;
        DR=0;
        // imitate Steem native, eg My Socks are Weapons
        // not documented; only restore?
#if defined(SSE_FLOPPY_EVENT2)
        update_time=time_of_next_event+1024;
#else
        update_time=ACT+1024; 
#endif
      }    
      else
       OnUpdate(); // some recursion is always cool   
      
    }
    break;

  case WD_TYPEI_SEEK: // 'A'
    DSR=DR;
    if(TR==DSR)
      prg_phase=WD_TYPEI_CHECK_VERIFY;
    else
    {
      Lines.direction=(DSR>TR);
      prg_phase=WD_TYPEI_STEP_UPDATE;
    }
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_STEP_UPDATE: // 'B'
    if(Lines.direction)
      TR++;
    else
      TR--;
    prg_phase=WD_TYPEI_STEP;
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_STEP: // 'C'
    Lines.track0=(floppy_head_track[DRIVE]==0);

    if(Lines.track0 && !Lines.direction)
    {
      TR=0;
      prg_phase=WD_TYPEI_CHECK_VERIFY;
      OnUpdate(); // some recursion is always cool
    }
    else
    {
      StepPulse();
/*
Delay according to r1,r0 field

Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
--------     -----     --     --     --     --     --     --     -----
Restore      0         0      0      0      h      V      r1     r0
Seek         0         0      0      1      h      V      r1     r0
Step         0         0      1      u      h      V      r1     r0
Step in      0         1      0      u      h      V      r1     r0
Step out     0         1      1      u      h      V      r1     r0

r1       r0            1772
--       --            ----
0        0             6000 cycles
0        1             12000 cycles
1        0             2000 cycles
1        1             3000 cycles
*/
      switch(CR&(CR_R1|CR_R0))
      {
      case 0:
        update_time=MsToCycles(6);
        break;
      case 1:
        update_time=MsToCycles(12);
        break;
      case 2:
        update_time=MsToCycles(2);
        break;
      case 3:
        update_time=MsToCycles(3);
        break;
      }//sw
#if defined(SSE_FLOPPY_EVENT2)
      update_time+=time_of_next_event;
#else
      update_time+=ACT;; 
#endif
      prg_phase=WD_TYPEI_STEP_PULSE;
    }
    break;

  case WD_TYPEI_STEP_PULSE:
    // goto 'D' if command is step, 'A' otherwise
    prg_phase=(CR&CR_STEP)?WD_TYPEI_CHECK_VERIFY:WD_TYPEI_SEEK;
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_CHECK_VERIFY: // 'D'

#ifdef SSE_WD1772_371
    // update STR bit 2 (reflects status of the TR00 signal)
    if(Lines.track0)
      STR|=STR_T00; // fixes Power Drift SCP disk A
    else
      STR&=~STR_T00; // fixes R-Type SCP
#endif

    if(CR&CR_V)
    {
      if(IMAGE_STW)
      {
        ImageSTW[DRIVE].LoadTrack(CURRENT_SIDE,SF314[DRIVE].Track());
      }
#if defined(SSE_DISK_SCP)
      else if(IMAGE_SCP)
      {
        ImageSCP[DRIVE].LoadTrack(CURRENT_SIDE,SF314[DRIVE].Track());
      }
#endif
#if defined(SSE_DISK_HFE)
      else if(IMAGE_HFE)
      {
        ImageHFE[DRIVE].LoadTrack(CURRENT_SIDE,SF314[DRIVE].Track());
      }
#endif

      prg_phase=WD_TYPEI_HEAD_SETTLE; 
#if defined(SSE_FLOPPY_EVENT2)
      update_time=time_of_next_event+ MsToCycles(15);
#else
      update_time=ACT+ MsToCycles(15);
#endif
    }
    else
    {
      Irq(true); // this updates status bits
    }
    break;

  case WD_TYPEI_HEAD_SETTLE:
    // flow chart is missing head settling
    prg_phase=WD_TYPEI_FIND_ID;
#if defined(SSE_WD1772_AM_LOGIC)
    Amd.Reset();
#endif
    n_format_bytes= n00=nFF=0;
    Read(); // drive will send word (clock, byte) and set event
    IndexCounter=6; 
//    TRACE_LOG("%d IP to find ID %d\n",IndexCounter,SR);
    break;

  case WD_TYPEI_FIND_ID:
  case WD_TYPEII_FIND_ID:
  case WD_TYPEIII_FIND_ID:
/*  From doc, sequence is 12x0, 3xA1, FE or FF, it must strictly be respected.
    But Platoon: it may be $FF instead of 00, 11 bytes may be enough, and
    a series of 11 x 0 + 1 x 2 is OK. We try to handle this (even if it doesn't
    help in Platoon, which is not copiable on a ST).
    There are certainly more variations possible.
    In fact we don't use the count?
    When looking for ID, the WD1772 ignores the last bit, so $FE is
    equivalent to $FF.
*/
    CrcLogic.Add(DSR);
//  TRACE("dsr=%x fmt=%d\n",DSR,n_format_bytes);

#if defined(SSE_WD1772_AM_LOGIC)
    // wait for AM
    if(Amd.aminfo & CAPSFDC_AI_DSRAM)
    {
      ASSERT(IMAGE_SCP);
      // AM detected, read returns on dsr ready
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
      Amd.nA1=3;
      CrcLogic.Reset(); 

#if defined(SSE_WD1772_AM_LOGIC) && SSE_VERSION>=372
      Amd.Enabled=false; // read IDs OK
#endif

    }
    else
#endif

    if(DSR==0xA1 && !(Mfm.clock&BIT_5)
#if ! defined(SSE_WD1772_371) // not $C2
      || DSR==0xC2 && !(Mfm.clock&BIT_4)  
#endif
#if defined(SSE_WD1772_AM_LOGIC) 
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      )
    {
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.nA1++;
#else
      n_format_bytes++;
#endif
#if ! defined(SSE_WD1772_371) // always A1, always reset
      if(DSR==0xA1)
#endif
        CrcLogic.Reset(); // only special $A1 resets the CRC logic

#if defined(SSE_WD1772_AM_LOGIC)
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
#endif
    }
#if SSE_VERSION<371 // no use
    else if(!n_format_bytes) // count zeroes (or ones)
    {
      if(!DSR || n00==11&&DSR==2)
      {
        n00++;
        nFF=0;
      }
      else if(DSR==0xFF)
      {
        n00=0;
        nFF++;
      }
      else
      {
        n00=nFF=0;
      }
    }
#endif
#if defined(SSE_WD1772_AM_LOGIC)
    else if( (DSR&0xFF)>=0xFC && Amd.nA1==3) // CAPS: $FC->$FF
#else
    else if( (DSR&0xFF)>=0xFC && n_format_bytes==3) // CAPS: $FC->$FF
#endif
    //else if( (DSR&0xFE)==0xFE) && n_format_bytes==3) // doc: $FE(+$FF)
    {
      TRACE_LOG("%X found at %d\n",DSR,SF314[DRIVE].BytePosition());
      n_format_bytes=n00=nFF=0;//reset
      prg_phase++; // in type I or type II or III
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.Enabled=false; // read IDs OK
#endif
    }
#if defined(SSE_WD1772_AM_LOGIC)
    else if(Amd.nA1)
      Amd.Reset();
#else
    else if(n_format_bytes)
      n_format_bytes=n00=nFF=0;
#endif

#if defined(SSE_DISK_SCP2B)
    // switch back to rev1 if we're on rev2
    else if(IMAGE_SCP && ImageSCP[DRIVE].rev)
      ImageSCP[DRIVE].LoadTrack(CURRENT_SIDE,CURRENT_TRACK);
#endif


    Read(); // this sets up next event
    break;

  case WD_TYPEI_READ_ID:
  case WD_TYPEII_READ_ID:
  case WD_TYPEIII_READ_ID:
    // fill in ID field
    *(((BYTE*)&IDField)+n_format_bytes)=DSR; //no padding!!!!!!
    if(n_format_bytes<4)
      CrcLogic.Add(DSR);
    if(prg_phase==WD_TYPEIII_READ_ID)
    {
      DR=DSR;
      Drq(true); // read address
    }
    n_format_bytes++;
    if(n_format_bytes==sizeof(TWD1772IDField))
    {
      n_format_bytes=0; 
      prg_phase++; // in type I, II, III
#ifdef SSE_DEBUG
      TRACE_LOG("At %d:",Disk[DRIVE].current_byte); // position
      IDField.Trace();
#endif
      OnUpdate(); // some recursion is always cool
    }
    else
      Read();
    break;

  case WD_TYPEI_TEST_ID:
#ifdef SSE_DEBUG____
    TRACE_LOG("At %d:",Disk[DRIVE].current_byte); // position
    IDField.Trace();
#endif
    //test track and CRC
    if(IDField.track==TR && CrcLogic.Check(&IDField))
    {
      CrcLogic.Reset();
      Irq(true); // verify OK
    }
    else // they should all have correct track, will probably time out
    {
      prg_phase=WD_TYPEI_FIND_ID;
      if(IDField.track==TR)
      {
        STR|=STR_CRC; // set CRC error if track field was OK
      }
      CrcLogic.Add(DSR); //unimportant
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.Enabled=true; 
#endif
      Read(); // this sets up next event
    }
    break;

  case WD_TYPEII_SPUNUP:
  case WD_TYPEIII_SPUNUP:
    prg_phase++;
    if(CR&CR_E) // head settle delay programmed
#if defined(SSE_FLOPPY_EVENT2)
      update_time=time_of_next_event+ MsToCycles(15);
#else
      update_time=ACT+ MsToCycles(15);
#endif
    else
      OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEII_HEAD_SETTLE: // we come directly or after 15ms delay
//    SF314[DRIVE].State.reading=true;
    // check Write Protect for command write sector
    if((CR&CR_TYPEII_WRITE) 
      && (Lines.write_protect|| FloppyDrive[DRIVE].ReadOnly) )
    {
      STR|=STR_WP;
      Irq(true);
    }
    else // read, or write OK
    {
      IndexCounter=5; 
      //TRACE_LOG("%d IP to find ID %d\n",IndexCounter,SR);
      prg_phase=WD_TYPEII_FIND_ID; // goto '1'
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.Reset();
#endif
      n_format_bytes=n00=nFF=0;
      Read();
    }
    break;

  case WD_TYPEII_TEST_ID:
    ASSERT(!n_format_bytes);
#ifdef SSE_DEBUG___
    ASSERT(!n_format_bytes);
    TRACE_LOG("At %d:",Disk[DRIVE].current_byte); // position
    IDField.Trace();
#endif
    if(IDField.track==TR && IDField.num==SR)
    {
      ByteCount=IDField.nBytes();
      if(CrcLogic.Check(&IDField))
      {
        CrcLogic.Reset();
        prg_phase=(CR&CR_TYPEII_WRITE) ? WD_TYPEII_WRITE_DAM :
          WD_TYPEII_FIND_DAM;
      }
      else
      {
        STR|=STR_CRC;
        CrcLogic.Add(DSR);
        prg_phase=WD_TYPEII_FIND_ID; 
      }
    }
    else // it's no error (yet), the WD1772 must browse the IDs
      prg_phase=WD_TYPEII_FIND_ID;
#if defined(SSE_WD1772_AM_LOGIC)
    Amd.Reset();
#endif
    Read();
    break;

  case WD_TYPEII_FIND_DAM:
    CrcLogic.Add(DSR);//before eventual reset
    n_format_bytes++;
    //TRACE_LOG("%d ",n_format_bytes);
#if defined(SSE_WD1772_AM_LOGIC)
    if(n_format_bytes<27)
      ; // CAPS: first bytes aren't even read
    else if(n_format_bytes==27)
    {
      TRACE_MFM("Enable AMD\n");
      Amd.Enable();
      Amd.amisigmask=CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRMA1;
    }
#else
    if(n_format_bytes<28)
      ; // CAPS: first bytes aren't even read
#endif

#if defined(SSE_WD1772_371)
    else if(n_format_bytes==44)
#else
    else if(n_format_bytes==43) //timed out
#endif
    {
      TRACE_LOG("DAM time out %d in\n",n_format_bytes);
      n_format_bytes=0;
      prg_phase=WD_TYPEII_FIND_ID;
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.Enable();
#endif
    }
#if defined(SSE_WD1772_AM_LOGIC)
    //if not A1 mark, restart
    else	if ( (IMAGE_SCP)
      && (Amd.aminfo & CAPSFDC_AI_DSRAM) && !(Amd.aminfo & CAPSFDC_AI_DSRMA1)
      && n_format_bytes>=43)
    {
      Amd.Enable();
      n_format_bytes=n00=0;
    }

    // wait for AM
    else if ((Amd.aminfo & CAPSFDC_AI_DSRAM))
    {
      TRACE_LOG("AM found at byte %d (%d in), reset CRC\n",DSR,Disk[DRIVE].current_byte,n_format_bytes);
      // AM detected, read returns on dsr ready
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
      CrcLogic.Reset();
      Amd.nA1=3;
    }
#endif

    else if(DSR==0xA1 && !(Mfm.clock&BIT_5) //stw
#if defined(SSE_WD1772_AM_LOGIC) //last minute 3.7.1
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      ) 
    {
      TRACE_LOG("%X found at byte %d, reset CRC\n",DSR,Disk[DRIVE].current_byte);
      CrcLogic.Reset();
#if defined(SSE_WD1772_371)
      Amd.nA1++; 
#endif
    }
#if defined(SSE_WD1772_371) 
    else if(Amd.nA1==3 && ((DSR&0xFE)==0xF8||(DSR&0xFE)==0xFA)) // DAM found
#else
    else if( (DSR&0xFE)==0xF8 ||  (DSR&0xFE)==0xFA ) // DAM found
#endif
    {
      TRACE_LOG("TR%d SR%d %X found at byte %d (%d after ID)\n",TR,SR,DSR,Disk[DRIVE].current_byte,n_format_bytes);
      n_format_bytes=0; // for CRC later
#if defined(SSE_WD1772_AM_LOGIC)
      Amd.Enabled=false;
#endif
      prg_phase=WD_TYPEII_READ_DATA;
      if((DSR&0xFE)==0xF8)
        STR|=STR_RT; // "record type" set when "deleted data" DAM
    }
#if defined(SSE_WD1772_371) 
    else if(Amd.nA1==3) // address mark but then no FB...
    {
      TRACE_LOG("%x found after AM: keep looking\n",DSR);
      Amd.Enable();
    }
#endif
    Read();    
    break;

  case WD_TYPEII_READ_DATA:
    CrcLogic.Add(DSR);
    DR=DSR;
    Drq(true); // DMA never fails to take the byte
    ByteCount--;
    ASSERT(!n_format_bytes);
    if(!ByteCount)
      prg_phase=WD_TYPEII_READ_CRC;
    Read();
    break;

  case WD_TYPEII_READ_CRC:
    IDField.CRC[n_format_bytes]=DSR; // and we don't add to CRC
    if(n_format_bytes) //1
    {
      if(!CrcLogic.Check(&IDField))
      {
        TRACE_LOG("Read sector %c:%d-%d-%d CRC error\n",'A'+DRIVE,CURRENT_SIDE,IDField.track,IDField.num);
        STR|=STR_CRC; // caught by Dma.UpdateRegs() for OSD
        Irq(true);
      }
      else
      {
        TRACE_LOG("Read sector %c:%d-%d-%d OK CRC %X%X\n",'A'+DRIVE,CURRENT_SIDE,IDField.track,IDField.num,IDField.CRC[0],IDField.CRC[1]);
        prg_phase=WD_TYPEII_CHECK_MULTIPLE;
        OnUpdate(); // some recursion is always cool
      }
    }
    else
    {
      n_format_bytes++;
      Read(); // next CRC byte
    }
    break;

  case WD_TYPEII_CHECK_MULTIPLE:
    if(CR&CR_M)
    {
      SF314[DRIVE].State.writing=0;
      SR++;
      if(!SR)
        SR++; // CAPS: 255 wraps to 1 (due to add with carry)
      prg_phase=WD_TYPEII_HEAD_SETTLE; // goto '4'
      OnUpdate();
    }
    else
      Irq(true);
    break;

  case WD_TYPEII_WRITE_DAM:
    n_format_bytes++;
    if(n_format_bytes<23-1) //22 or 23?
      Read();    
    else if(n_format_bytes<23-1+12) // write 12 $0
    {
      Lines.write_gate=1; // those lines don't matter for now
      Lines.write=1;
      Mfm.data=0;
      Mfm.Encode(); 
      //TRACE_FDC("write %X at byte %d\n",Mfm.data,Disk[DRIVE].current_byte);
      CrcLogic.Add(Mfm.data); // shouldn't matter
      Write();
    }
    else if(n_format_bytes<23-1+12+3) // write 3x $A1 (missing in flow chart)
    {
      Mfm.data=0xA1;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK); 
      //TRACE_FDC("write %X at byte %d, reset CRC\n",Mfm.data,Disk[DRIVE].current_byte);
      CrcLogic.Add(Mfm.data); // before reset   
      CrcLogic.Reset();
      Write();
    }
    else if(n_format_bytes==23-1+12+3) // write DAM acording to A0 field
    {
      ASSERT(!(CR&CR_A0)); //Amateur versions (>1.50?) of ProCopy use $A1 to copy
      Mfm.data= (CR&CR_A0)? 0xF9 : 0xFB;
      Mfm.Encode(); 
      TRACE_LOG("TR %d SR %d write %X at byte %d, %d in\n",TR,SR,Mfm.data,Disk[DRIVE].current_byte,n_format_bytes);
      CrcLogic.Add(Mfm.data); // after eventual reset (TODO)        
      Write();     
    }
    else
    {
      n_format_bytes=0;
      prg_phase=WD_TYPEII_WRITE_DATA;
      OnUpdate(); // some recursion is always cool  
    }
    break;

  case WD_TYPEII_WRITE_DATA:
    Drq(true); // normally first DRQ happened much earlier, we simplify
    DSR=DR;
    CrcLogic.Add(DSR);
    Mfm.data=DSR;
    Mfm.Encode();
    ByteCount--;
    ASSERT(!n_format_bytes);
    if(!ByteCount)
    {
      prg_phase=WD_TYPEII_WRITE_CRC;
      TRACE_FDC("CRC: %X\n",CrcLogic.crc);
    }
    Write();
    break;

  case WD_TYPEII_WRITE_CRC: // CRC + final $FF (?)
    n_format_bytes++;
    if(n_format_bytes==1)
      Mfm.data=CrcLogic.crc>>8;
    else if(n_format_bytes==2)
    {
      Mfm.data=CrcLogic.crc&0xFF;
    }
    else
    {
      n_format_bytes=0;
      prg_phase=WD_TYPEII_CHECK_MULTIPLE;
      Mfm.data=0xFF;
      Lines.write_gate=0; //early
      Lines.write=0; // early
    }
    Mfm.Encode();
    Write();
    break;

  case WD_TYPEIII_HEAD_SETTLE: // we come directly or after 15ms delay

#if defined(SSE_WD1772_AM_LOGIC)
    Amd.Reset();
    Amd.aminfo&=~CAPSFDC_AI_CRCENABLE;
    Amd.amisigmask=CAPSFDC_AI_DSRREADY;
#endif

    if((CR&0xF0)==CR_TYPEIII_READ_ADDRESS)
    {
      IndexCounter=5; //not documented, see OnIndexPulse()
      prg_phase=WD_TYPEIII_FIND_ID;
      n_format_bytes=n00=nFF=0;
      Read();
    }
    // check Write Protect for command write track
    else if((CR&CR_TYPEIII_WRITE) 
      && (Lines.write_protect|| FloppyDrive[DRIVE].ReadOnly) )
    {
      TRACE_LOG("Can't write on disk\n");
      STR|=STR_WP;
      Irq(true);
    }
    else // for read & write track, we start at next IP
    {
      IndexCounter=1;
  //    TRACE_LOG("%d IP for read or write track\n",IndexCounter);
      prg_phase=WD_TYPEIII_IP_START;
      ASSERT( SF314[DRIVE].State.motor );
    }
    break;

  case WD_TYPEIII_TEST_ID:
    if(!CrcLogic.Check(&IDField))
      STR|=STR_CRC;
    SR=IDField.track;
    Irq(true);
    break;

  case WD_TYPEIII_READ_DATA: 

    // "the Address Mark Detector is on for the duration of the command"
    if(DSR==0xA1 && !(Mfm.clock&BIT_5)
#if defined(SSE_WD1772_AM_LOGIC) //!!!!!!!!!!!! last minute
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      )
    {
#if defined(SSE_DISK_SCP) // don't need this hack with the SCP version
      if(!IMAGE_SCP)// CAPS-like code produces the $14, it's quite intricate
#endif
      if(CrcLogic.crc!=0xCDB4)
        DSR=0x14; // 1st AM doesn't read as $A1: Union Demo
      CrcLogic.Reset();
    }
    else
      CrcLogic.Add(DSR); 
   
    DR=DSR;
    Drq(true);
    Read();
    break;
//#undef SSE_WD1772_F7_ESCAPE    
  case WD_TYPEIII_WRITE_DATA:  
    // The most interesting part of STW support, and novelty in ST emulation!
    Drq(true);
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS // so we'll trace all written IDs
    if((DR&0xFE)==0xFE
#if defined(SSE_BOILER_TRACE_CONTROL) && SSE_VERSION>=372
      && !(TRACE_MASK3&(TRACE_CONTROL_FDCMFM|TRACE_CONTROL_FDCBYTES))
#endif
      )
      n_format_bytes=4;
#endif
    // analyse byte in for MFM markers
#if defined(SSE_WD1772_F7_ESCAPE)
    if(DR==0xF5 && !F7_escaping)
#else
    if(DR==0xF5) //Write A1 in MFM with missing clock Init CRC
#endif
    {
      Mfm.data=DSR=0xA1;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
      CrcLogic.Reset();
      Write();
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS
      if(n_format_bytes)
      {
        TRACE_LOG("$%x-",DSR);
        n_format_bytes--;
      }
#endif
    }
#if defined(SSE_WD1772_F7_ESCAPE)
    else if(DR==0xF6 && !F7_escaping)
#else
    else if(DR==0xF6) //Write C2 in MFM with missing clock
#endif
    {
      Mfm.data=DSR=0xC2;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
      CrcLogic.Add(Mfm.data);
      Write();
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS
      if(n_format_bytes)
      {
        TRACE_LOG("$%x-",DSR);
        n_format_bytes--;
      }
#endif

    }
/*  The format code $F7 may be used inside an ID field. The CRC bytes are added
    to the CRC, so that this is correct. This implies that at the receipt of
    $F7, the WD1772 saves the current value of the CRC (at least the lower 
    byte), before it is modified by the upper byte.
    $F7 will trigger output of CRC only if the CRC is non null (Dragonflight
    track headers).
    Notice that the CRC is null if after a reset you output it:
    $F5 -> $A1, CRC = $CDB4
    $F7 -> $CD, $B4, CRC = 0
    $F7 -> $F7
    So the 2nd $F7 is really written $F7 on the disk.
    Update:
    In fact, a byte following F7 isn't interpreted as a format byte, that's why
    the 2nd F7 is written F7, you can write F5, F6 the same way.
    http://thethalionsource.w4f.eu/Artikel/fua.htm 
*/
#if defined(SSE_WD1772_F7_ESCAPE)
    else if(DR==0xF7 && !F7_escaping)
#else
    else if(DR==0xF7 && CrcLogic.crc) //Write 2 CRC Bytes
#endif
    {
//      TRACE("write CRC %X %d %d\n",CrcLogic.crc,HIBYTE(CrcLogic.crc),LOBYTE(CrcLogic.crc));
      Mfm.data=DSR=CrcLogic.crc>>8; // write 1st byte
      DR=CrcLogic.crc&0xFF; // save 2nd byte
      CrcLogic.Add(Mfm.data);
#if defined(SSE_WD1772_372)
      Mfm.Encode(); // of course, silly bug
#else
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
#endif
#if defined(SSE_WD1772_F7_ESCAPE)
      F7_escaping=true;
#endif
      Write();
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS
      if(n_format_bytes)
      {
        TRACE_LOG("%d-",DSR);
        n_format_bytes--;
      }
#endif

      prg_phase=WD_TYPEIII_WRITE_DATA2; // for 2nd byte
    }
    else // other bytes ($0, $E5...)
    {
      Mfm.data=DSR=DR;
#if defined(SSE_WD1772_372)
      Mfm.Encode(); // of course, silly bug
#else
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
#endif
      CrcLogic.Add(DSR);
#if defined(SSE_WD1772_F7_ESCAPE)
      F7_escaping=false;
#endif
      Write();
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS
      if(n_format_bytes&&DR!=0xFE)
      {
        TRACE_LOG("%d ",DSR);
        if(n_format_bytes==1)
          TRACE_LOG("\n");
        n_format_bytes--;
      }
#endif

    }
    break;

  case WD_TYPEIII_WRITE_DATA2:
    // write 2nd byte of CRC
    Mfm.data=DSR=DR;// as saved
    CrcLogic.Add(Mfm.data);
#if defined(SSE_WD1772_372)
    Mfm.Encode(); // of course, silly bug
#else
    Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
#endif
    Write(); 
#ifdef SSE_DEBUG_WRITE_TRACK_TRACE_IDS
    if(n_format_bytes)
    {
      TRACE_LOG("%d-",DSR);
      n_format_bytes--;
    }
#endif
    prg_phase=WD_TYPEIII_WRITE_DATA; // go back
    break;
  }//sw
}

#endif


void TWD1772::Read() {
#if SSE_VERSION>=372
  ASSERT(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE);
#endif
  if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE)
  {
    SF314[DRIVE].Read(); // this gets data and creates event
    Mfm.Decode();
#if defined(SSE_WD1772_AM_LOGIC) 
    if(!IMAGE_SCP) // DSR shouldn't be messed with...
#endif
      DSR=Mfm.data;
#ifdef SSE_DEBUG // for SCP it's not perfectly aligned
    TRACE_MFM("%s #%d MFM %04X c $%02X d $%02X\n",wd_phase_name[prg_phase],Disk[DRIVE].current_byte,Mfm.encoded,Mfm.clock,DSR);
#endif
  }
}


void TWD1772::StepPulse() {
/*
  // useless now, normally it lasts some �s, but we're not going to
  // set up events for that
  Lines.step=true; 
  Lines.step=false;
*/
  if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE)
    SF314[YM2149.SelectedDrive].Step(Lines.direction);
}


void TWD1772::Write() {
/*  Data must be MFM-encoded before.
    We don't do it here because we don't know if we code for special
    format bytes or not.
*/
#ifdef SSE_DEBUG
  TRACE_MFM("%s #%d MFM %04X c $%02X d $%02X\n",wd_phase_name[prg_phase],Disk[DRIVE].current_byte,Mfm.encoded,Mfm.clock,Mfm.data);
  ASSERT(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE);
#endif
  if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE)
    SF314[DRIVE].Write(); // this writes data and creates event  
}


// TWD1772IO -> WriteCR when manager=MNGR_WD1772


void  TWD1772::WriteCR(BYTE io_src_b) {
  ASSERT(IMAGE_STW||IMAGE_SCP||IMAGE_HFE);
  ASSERT( SF314[DRIVE].ImageType.Manager==MNGR_WD1772 );

  if(CommandType(io_src_b)==2 || CommandType(io_src_b)==3) //or no condition?
  {
    // there's certainly a more elegant C++ way... TODO!
    if(IMAGE_STW)
      ImageSTW[DRIVE].LoadTrack(floppy_current_side(),floppy_head_track[DRIVE]);
#if defined(SSE_DISK_SCP)
    else if(IMAGE_SCP)
      ImageSCP[DRIVE].LoadTrack(floppy_current_side(),floppy_head_track[DRIVE]);
#endif
#if defined(SSE_DISK_HFE)
    else if(IMAGE_HFE)
      ImageHFE[DRIVE].LoadTrack(floppy_current_side(),floppy_head_track[DRIVE]);
#endif
  }

/*  CR will accept a new command when busy bit is clear or when command
    is 'Force Interrupt'.
    Not documented: also when the drive is still spinning up 
    TODO only during a type I command? check in caps -> feature missing?
    TODO check general structure, not sure it's ideal even if limited
    to STW
*/
  if(!(STR&STR_BUSY) 
    || prg_phase==WD_TYPEI_SPINUP
    || prg_phase==WD_TYPEII_SPINUP
    || prg_phase==WD_TYPEIII_SPINUP
    || (io_src_b&0xF0)==0xD0 )
  {
    agenda_delete(agenda_fdc_motor_flag_off); // and others?
#if defined(SSE_DMA_TRACK_TRANSFER) && !defined(SSE_DMA_TRACK_TRANSFER2)
    Dma.Datachunk=0;
#endif
    NewCommand(io_src_b); // one more function, more readable
  }
  else
  {
    TRACE_LOG("FDC command %X ignored\n",io_src_b);
  }
}

#endif

#if defined(SSE_WD1772_AM_LOGIC)
/*  This is the correct algorithm for the WD1772 data separator.
    It interprets the bit flow from disk images such as SCP, coming
    from the DPLL.
    
    Fluxes -> weak bit detect (hack) -> DPLL -> data separator -> DSR

    Thx to Istvan Fabian for some inspiration otherwise Steem would FAIL
    (err, why write in CAPS?) on some disk images that use the $C2 sync mark,
    like Albedo and Jupiter's Masterdrive.
    Note: my comments in this function marked by SS
*/

bool TWD1772::ShiftBit(int bit) {

  bool byte_ready=false;

  // shift read bit into am decoder
  DWORD amdecode=Amd.amdecode<<1;
  if (bit)
    amdecode|=1;
  Amd.amdecode=amdecode;

  // get am info, clear AM found, A1 and C2 mark signals
  DWORD aminfo=Amd.aminfo & ~(CAPSFDC_AI_AMFOUND|CAPSFDC_AI_MARKA1|CAPSFDC_AI_MARKC2);

  // bitcell distance of last mark detected
  if (Amd.ammarkdist)
    Amd.ammarkdist--;

  // am detector if enabled
  if(Amd.Enabled) {
    //if (aminfo & CAPSFDC_AI_AMDETENABLE) { //SS TODO
    //ASSERT(aminfo & CAPSFDC_AI_AMDETENABLE); //SS asserts...
    // not a mark in shifter/decoder
    int amt=0;

    // check if shifter/decoder has a mark
    // the real hardware probably has two shifters (clocked and fed by data separator) connected to a decoder
    // each bit of the shifter/decoder is for shifter0#0, shifter1#0...shifter0#7, shifter1#7, 
    // only 2 comparisons is needed per cell instead of 8 (A1/0A, C2/14 and 0A/A1, 14/C2)
    switch (amdecode  & 0xffff) {
      // A1 mark only enabled if not overlapped with another A1
      case 0x4489:
        if (!Amd.ammarkdist || Amd.ammarktype!=1)
          amt=1;
        break;

        // C2 mark always enabled
      case 0x5224:
        amt=2;
        break;
    }

    // process mark if found
    if (amt) {
      // we just read the last data bit of a mark, delay by a clock bit to read from decoder#1
      Amd.amdatadelay=1;

      // if overlapped with a different mark
      if (Amd.ammarkdist && Amd.ammarktype!=amt) {
        // dsr value is invalid
        Amd.amdataskip++;

        // delay by an additional data bit (data, clock)
        Amd.amdatadelay+=2;
      }

      // if dsr is empty dsr shouldn't be flushed, dsr value is invalid
      if (!Amd.dsrcnt)
        Amd.amdataskip++;

      // force dsr to flush its current value, since data values start from next data bit
      Amd.dsrcnt=7;

      // 16 bitcells must be read before next mark, otherwise marks are overlapped (8 clock+data bits)
      Amd.ammarkdist=16;

      // save last mark type; only used when marks overlap
      Amd.ammarktype=amt;

      // set mark signal, the shifter/decoder must be connected to the crc logic, not dsr
      if (amt == 1) {
        aminfo|=CAPSFDC_AI_MARKA1|CAPSFDC_AI_MA1ACTIVE;


        // if CRC is enabled, first A1 mark activates the CRC logic
        if (aminfo & CAPSFDC_AI_CRCENABLE) {
          // if CRC logic is not activate yet reset CRC value and counter, 16 cells already available as mark
          if (!(aminfo & CAPSFDC_AI_CRCACTIVE)) {
            aminfo|=CAPSFDC_AI_CRCACTIVE;
            //pc->crc=~0; //SS keep ours
            CrcLogic.crccnt=16; 
          }
        }

      } else
        aminfo|=CAPSFDC_AI_MARKC2;
      TRACE_MFM(" mark %X ",(amdecode  & 0xffff));
    }
  }


  // process CRC if activated
  if (aminfo & CAPSFDC_AI_CRCACTIVE) {
    // process new value at every 16 cells (8 clock/data)
    if (!(CrcLogic.crccnt & 0xf)) { 
      // reset CRC process if less than 3 consecutive A1 marks detected
      if (CrcLogic.crccnt>48 || (aminfo & CAPSFDC_AI_MARKA1)) {
        // 3 consecutive A1 marks found: set AM found signal, disable AM detector
        if (CrcLogic.crccnt == 48) {
          aminfo|=CAPSFDC_AI_AMFOUND|CAPSFDC_AI_AMACTIVE;
          aminfo&=~CAPSFDC_AI_AMDETENABLE;
        }
      } else
        aminfo&=~(CAPSFDC_AI_CRCACTIVE|CAPSFDC_AI_AMACTIVE);
    }

    CrcLogic.crccnt++;
  }



  // wait for data clock cycle plus bitcell delay
  if (!Amd.amdatadelay) {
    // set next delay
    // just read a clock bit here, next cell is data, that gets processed at the clock bit after that
    Amd.amdatadelay=1;

    // clear dsr signals
    aminfo&=~(CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRAM|CAPSFDC_AI_DSRMA1);

    // shift data bit into dsr, this is a clock bit, data bit is at decoder#1
    Amd.dsr=((Amd.dsr<<1) | (amdecode>>1 & 1)) & 0xff;

    // process data if 8 bits are dsr now, otherwise just increase dsr counter
    if (++Amd.dsrcnt == 8) {
      // reset dsr counter
      Amd.dsrcnt=0;

      // if AM found set dsr signal
      if (aminfo & CAPSFDC_AI_AMACTIVE) {
        TRACE_MFM(" -AM- ");
        aminfo&=~CAPSFDC_AI_AMACTIVE;
        aminfo|=CAPSFDC_AI_DSRAM;
      }

      // if A1 mark found set dsr signal
      if (aminfo & CAPSFDC_AI_MA1ACTIVE) {
        aminfo&=~CAPSFDC_AI_MA1ACTIVE;
        aminfo|=CAPSFDC_AI_DSRMA1;
      }

      // set dsr ready signal, unless data is invalid
      if (!Amd.amdataskip)
      {
        aminfo|=CAPSFDC_AI_DSRREADY;
        DSR=(Amd.dsr&0xFF);
      }
      else
        Amd.amdataskip--;
    }
  } else
  {
    Amd.amdatadelay--;
#ifdef SSE_DEBUG
    Mfm.encoded=(WORD)Amd.amdecode; //wrong byte/clock order for 1st $A1
#endif
  }

  // save new am info
  Amd.aminfo=aminfo;

  // if a byte is complete, break and signal new byte
  //  if (Amd.aminfo & Amd.amisigmask) { //SS hangs...
  if (Amd.aminfo & CAPSFDC_AI_DSRREADY) { //SS ?
    byte_ready=true;
#if defined(SSE_DISK_380) // are we taking a risk?
    if(Disk[DRIVE].current_side!=CURRENT_SIDE && SSE_HACKS_ON)
      Amd.dsr=rand()&0xFF; // garbage
#endif
  }
  return byte_ready;
}

#endif//precise sync

#endif//WD1772