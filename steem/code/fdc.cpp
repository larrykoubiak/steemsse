/*---------------------------------------------------------------------------
FILE: fdc.cpp
MODULE: emu
DESCRIPTION: Steem's Floppy Disk Controller (WD1772) core. floppy_fdc_command
is called when a command is written to the FDC's command register via the
DMA I/O addresses (see iow.cpp).
---------------------------------------------------------------------------*/

#if defined(SSE_COMPILER_INCLUDED_CPP)
#pragma message("Included for compilation: fdc.cpp")
#endif

#if defined(SSE_BUILD)

#define EXT
#define INIT(s) =s

#if defined(SSE_VAR_RESIZE)
EXT BYTE floppy_mediach[2];
EXT BYTE num_connected_floppies INIT(2);
#else
EXT int floppy_mediach[2];
EXT int num_connected_floppies INIT(2);
#endif

EXT int floppy_current_drive();

EXT BYTE floppy_current_side();
EXT void fdc_add_to_crc(WORD &,BYTE);

#if !defined(SSE_DMA)
EXT MEM_ADDRESS dma_address;
WORD dma_mode;
BYTE dma_status;
int dma_sector_count;
int dma_bytes_written_for_sector_count=0;
#endif

#if defined(SSE_FLOPPY_ALWAYS_ADAT)
EXT const bool floppy_instant_sector_access INIT(false);
#else
EXT bool floppy_instant_sector_access INIT(true);
#endif

#if defined(SSE_FDC_NOFF)
const EXT bool floppy_access_ff INIT(0);
#else
EXT bool floppy_access_ff INIT(0);
#endif
#if !defined(SSE_OSD_DRIVE_LED3)
EXT DWORD disk_light_off_time INIT(0);
#endif
EXT bool floppy_access_started_ff INIT(0);

#if USE_PASTI
EXT HINSTANCE hPasti INIT(NULL);
#if defined(SSE_TIMINGS_CPUTIMER64) //TODO: if/when pasti64 is available
EXT COUNTER_VAR pasti_update_time;
#else
EXT int pasti_update_time;
#endif
EXT const struct pastiFUNCS *pasti INIT(NULL);
EXT char pasti_file_exts[160];
EXT WORD pasti_store_byte_access;
EXT bool pasti_active INIT(0);
#endif

#if !defined(SSE_WD1772_REGS_FOR_FDC)
BYTE fdc_cr,fdc_tr,fdc_sr,fdc_str,fdc_dr; // made struct
bool fdc_last_step_inwards_flag;
#endif

BYTE floppy_head_track[2];

#if defined(SSE_VAR_RESIZE)
BYTE floppy_access_ff_counter=0;
BYTE floppy_irq_flag=0;
BYTE fdc_step_time_to_hbls[4]={94,188,32,47};
#if !defined(SSE_DMA_FIFO_FDC_READ_ADDRESS)
BYTE fdc_read_address_buffer_len=0;
#endif
WORD floppy_write_track_bytes_done;
BYTE fdc_spinning_up=0;
#if !defined(SSE_WD1772)
BYTE floppy_type1_command_active=2;  // Default to type 1 status
#endif
#else //var_resize
int floppy_access_ff_counter=0;
int floppy_irq_flag=0;
int fdc_step_time_to_hbls[4]={94,188,32,47};
int fdc_read_address_buffer_len=0;
int floppy_write_track_bytes_done;
int fdc_spinning_up=0;
#if !defined(SSE_WD1772)
int floppy_type1_command_active=2;  // Default to type 1 status
#endif
#endif //var_resize

#if !defined(SSE_DMA_FIFO_FDC_READ_ADDRESS) //argh, forgot switch until 392
EXT BYTE fdc_read_address_buffer[20];
#endif

#undef EXT
#undef INIT

#else//decla

#define DMA_ADDRESS_IS_VALID_R (dma_address<himem)
#define DMA_ADDRESS_IS_VALID_W (dma_address<himem && dma_address>=MEM_FIRST_WRITEABLE)

#endif//decla

#if defined(SSE_DMA_FIFO_FDC)

#define DMA_INC_ADDRESS Dma.IncAddress();

#else

#define DMA_INC_ADDRESS                                    \
  if (dma_sector_count){                                   \
    dma_address++;                                         \
    dma_bytes_written_for_sector_count++;                  \
    if (dma_bytes_written_for_sector_count>=512){        \
      dma_bytes_written_for_sector_count=0;              \
      dma_sector_count--;                                  \
      dma_status|=BIT_1;  /* DMA sector count not 0 */   \
      if (dma_sector_count==0) dma_status&=~BIT_1;     \
    }                                                      \
  }
#endif

#if defined(SSE_DRIVE)
#define FDC_HBLS_PER_ROTATION  (ADAT?SF314[DRIVE].HblsPerRotation():(313*50/5))
#else
// 5 revolutions per second, 313*50 HBLs per second
#define FDC_HBLS_PER_ROTATION (313*50/5) 
#endif

#if defined(SSE_FDC_ACCURATE_TIMING) 
// 4ms =200ms/50
#define FDC_HBLS_OF_INDEX_PULSE (FDC_HBLS_PER_ROTATION/ ((ADAT)?50:20)) 
#else
// 5% of track is index pulse, too high? 
#define FDC_HBLS_OF_INDEX_PULSE (FDC_HBLS_PER_ROTATION/20) // 20
#endif

#define LOGSECTION LOGSECTION_FDC
//---------------------------------------------------------------------------
int floppy_current_drive()
{
  if ((psg_reg[PSGR_PORT_A] & BIT_1)==0){ // Drive A
    return 0;
  }else if ((psg_reg[PSGR_PORT_A] & BIT_2)==0){ // Drive B
    return 1;
  }
  //SS this is fine for safety but see floppy_fdc_command for emulation
  return 0; // Neither, guess A 
}
//---------------------------------------------------------------------------
BYTE floppy_current_side()
{
  return (psg_reg[PSGR_PORT_A] & BIT_0)==0;
}
//---------------------------------------------------------------------------
BYTE read_from_dma()
{
  if (DMA_ADDRESS_IS_VALID_R){
    DEBUG_CHECK_READ_B(dma_address);
    return PEEK(dma_address);
  }
  return 0xff;
}
//---------------------------------------------------------------------------
void write_to_dma(int Val,int Num=1)
{
  int n=Num;
  if (Num<=0) n=1;
  for (int i=0;i<n;i++){
    if (dma_sector_count==0) break;

    if (DMA_ADDRESS_IS_VALID_W){
#if defined(SSE_BOILER_MONITOR_VALUE2)
      PEEK(dma_address)=BYTE(Val);
      DEBUG_CHECK_WRITE_B(dma_address);
#else
      DEBUG_CHECK_WRITE_B(dma_address);
      PEEK(dma_address)=BYTE(Val);
#endif
    }
    if (Num<=0) break;
    DMA_INC_ADDRESS;
  }
}
//---------------------------------------------------------------------------
bool floppy_handle_file_error(int floppyno,bool Write,int sector,int PosInSector,bool FromFormat)
{
  static DWORD last_reinsert_time[2]={0,0};
  TFloppyImage *floppy=&FloppyDrive[floppyno];

  log_write(EasyStr("File error - re-inserting disk ")+LPSTR(floppyno ? "B":"A"));

  bool WorkingNow=0;
  if (timer>=last_reinsert_time[floppyno]+2000 && floppy->DiskInDrive()){
    // Over 2 seconds since last failure
    FILE *Dest=NULL;
    if (FromFormat){
      if (floppy->ReopenFormatFile()) Dest=floppy->Format_f;
    }else{
      if (floppy->ReinsertDisk()) Dest=floppy->f;
    }
    if (Dest){
      if (floppy->SeekSector(floppy_current_side(),floppy_head_track[floppyno],sector,FromFormat)==0){
        fseek(Dest,PosInSector,SEEK_CUR);
        BYTE temp=read_from_dma();
        if (Write){
          WorkingNow=fwrite(&temp,1,1,Dest);
        }else{
          WorkingNow=fread(&temp,1,1,Dest);
        }
        if (DMA_ADDRESS_IS_VALID_W && dma_sector_count) write_to_dma(temp,0);
      }
    }else{
      GUIDiskErrorEject(floppyno);
    }
  }
  last_reinsert_time[floppyno]=timer;

  return NOT WorkingNow;
}
//---------------------------------------------------------------------------
bool floppy_track_index_pulse_active()
{
#if defined(SSE_FDC_ACCURATE_TIMING)
/*  Normally floppy_type1_command_active is up-to-date both in
    Steem native and WD1772 emu.
    Of course, selected drive should be spinning (motor on).
    Reporting IP is important for Panzer check rotation.
    This function is used by Steem native and by WD1772 manager.
*/
  if(ADAT)
  {
    bool active=(floppy_type1_command_active && SF314[DRIVE].State.motor
      && SF314[DRIVE].BytePosition()<=125); //4ms = +-125 bytes
    return active;
  }
  else 
#endif
  if (floppy_type1_command_active){
#if defined(SSE_FDC_394)
    if(num_connected_floppies!=DRIVE)
#endif
#if defined(SSE_VS2008_WARNING_382) 
    return ((hbl_count) % FDC_HBLS_PER_ROTATION)
      >=(DWORD)(FDC_HBLS_PER_ROTATION-FDC_HBLS_OF_INDEX_PULSE);
#else
    return (((DWORD)hbl_count) % FDC_HBLS_PER_ROTATION)>=(FDC_HBLS_PER_ROTATION-FDC_HBLS_OF_INDEX_PULSE);
#endif
  }
  return 0;
}
//---------------------------------------------------------------------------
void fdc_type1_check_verify()
{
#if defined(SSE_FDC_VERIFY_AGENDA)
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST) && !defined(SSE_BUGFIX_394)
  if(true)
#else
  if(ADAT)
#endif
  {
    if(FDC_VERIFY
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST) && !defined(SSE_BUGFIX_394)
      && ADAT
#endif
      )
    {
      WORD HBLsToNextID;
      BYTE NextIDNum;
      Disk[DRIVE].NextID(NextIDNum,HBLsToNextID); // C++ references
      HBLsToNextID+=MILLISECONDS_TO_HBLS(15); // head settling

#if defined(SSE_DRIVE_EMPTY_VERIFY_TIME_OUT)
/*  When you boot a ST with no disk into the drive, it will hang for
    a long time then show the desktop with two drive icons.
    It is handled here and not in SSEWD1772 as there's no disk image,
    'Steem native' is the default manager.
*/
      if(FloppyDrive[DRIVE].Empty())
      {
        TRACE_LOG("No disk %c verify times out\n",'A'+DRIVE);
      }
      else
#endif
      {
        WD1772.IndexCounter=0; // 5 REVS to find match
        agenda_add(agenda_fdc_verify,HBLsToNextID,NextIDNum);
      }
    }
    else
      // no verify: delay 'finish' to avoid nasty bugs (European Demos)
      agenda_add(agenda_fdc_verify,2,1); 
    return;
  }
#endif
  if (FDC_VERIFY==0) return;
  // This reads an ID field and checks that track number matches floppy_head_track
  // It will fail on an unformatted track or if there is no disk of course
  int floppyno=floppy_current_drive();
  TFloppyImage *floppy=&FloppyDrive[floppyno];
  if (floppy_head_track[floppyno]>FLOPPY_MAX_TRACK_NUM || floppy->Empty()){
    fdc_str|=FDC_STR_SEEK_ERROR;
  }else if (floppy->TrackIsFormatted[floppy_current_side()][floppy_head_track[floppyno]]==0){
    // If track is formatted then it is okay to seek to it, otherwise do this:
    if (floppy_head_track[floppyno]>=floppy->TracksPerSide) fdc_str|=FDC_STR_SEEK_ERROR;
    if (floppy_current_side() >= floppy->Sides) fdc_str|=FDC_STR_SEEK_ERROR;
  }
DEBUG_ONLY( if (fdc_str & FDC_STR_SEEK_ERROR) dbg_log("     Verify failed (track not formatted)"); )
}
//---------------------------------------------------------------------------
/* SS writes to $FF8604 when bits 2&1 of $FF8606 have been cleared
                WD1772 CR                DMA CR
   directly come here
   In some cases the write may be blocked (register not changed)
*/
void floppy_fdc_command(BYTE cm)
{
  dbg_log(Str("FDC: ")+HEXSl(old_pc,6)+" - executing command $"+HEXSl(cm,2));

#if defined(SSE_FDC_ACCURATE_BEHAVIOUR) && !defined(SSE_FDC_393A)
  if(ADAT && YM2149.Drive()==TYM2149::NO_VALID_DRIVE && (cm&0xF0)!=0xd0)
  {
    TRACE_LOG("CR %X no drive selected\n",cm);
    return; // time out
  }
#endif

  if (fdc_str & FDC_STR_BUSY){
#if defined(SSE_BUGFIX_394)
/*  Froggies broken!
    Quick fix: only commands of type 1 can be interrupted.
*/
    if(fdc_spinning_up &&!(WD1772.CR&BIT_3) 
      && WD1772.CommandType()==1  && cm!=WD1772.CR)
    {
      TRACE_LOG("CR %X->%X during spin-up\n",WD1772.CR,cm);
    }
    else
#elif defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*
  The doc states:
  "Command Register (CR)
  This register is not loaded when the device is busy unless the new command 
  is a force interrupt."
  But while the disk is still spinning up, it's possible to load a new command.
  I have seen this in Hatari but not in Kryoflux.
  This fixes Overdrive by Phalanx being stuck on Amiga disk.
  It wouldn't work on commands that start wihtout waiting (bit 3 'h' set), eg
  DSOS.
*/
    if(
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
      ADAT &&
#endif
      fdc_spinning_up &&!(WD1772.CR&BIT_3) 
      && WD1772.CommandType(cm)<4  && cm!=WD1772.CR)
      // we don't return, we will execute this command instead
      TRACE_LOG("CR %X->%X during spin-up\n",WD1772.CR,cm);
    else
#endif
    if ((cm & (BIT_7+BIT_6+BIT_5+BIT_4))!=0xd0){ // Not force interrupt
      dbg_log("     Command ignored, FDC is busy!");
      TRACE_LOG("Command %X ignored\n",cm);
      return;
    }
  }

#if defined(SSE_FDC_FORCE_INTERRUPT)
  // see note in SSEWD1772.cpp, IORead()
  if(
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
    !ADAT || 
#endif
    WD1772.InterruptCondition!=8)
    mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
  WD1772.InterruptCondition=0;
#else 
  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
#endif

  agenda_delete(agenda_fdc_finished);

  floppy_irq_flag=0;
#if !defined(SSE_FDC_394) // WD1772 will still start the commands (Audio Sculpture STF)
  if (floppy_current_drive()==1 && num_connected_floppies<2){ // Drive B disconnected?
    return;
  }
#endif
#if !defined(SSE_OSD_DRIVE_LED3)
  disk_light_off_time=timer+DisableDiskLightAfter;
#endif
  fdc_cr=cm;

  agenda_delete(agenda_fdc_motor_flag_off);
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*  No need to start the motor, it should already be spinning up
    (Noughts and Mad Crosses STE)
    But we need to start the motor if command interrupt (Froggies). 
*/
  if(
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
    ADAT && 
#endif
    (fdc_str&FDC_STR_BUSY) && WD1772.CommandType(cm)<4) 
    return;
#endif

  // AFAIK the FDC turns the motor on automatically whenever it receives a command.
  // Normally the FDC delays execution until the motor has reached top speed but
  // there is a bit in type 1, 2 and 3 commands that will make them execute
  // while the motor is in the middle of spinning up (BIT_3).
  bool delay_exec=0;

#if defined(SSE_FDC_394)
  if(num_connected_floppies!=DRIVE) // no motor if no drive
  {
#endif

  if ((fdc_str & FDC_STR_MOTOR_ON)==0){
    if ((cm & (BIT_7+BIT_6+BIT_5+BIT_4))!=0xd0){ // Not force interrupt
      if ((cm & BIT_3)==0){
        delay_exec=true; // Delay command until after spinup
      }
    }
    fdc_str=FDC_STR_BUSY | FDC_STR_MOTOR_ON;
    fdc_spinning_up=int(delay_exec ? 2:1);
#if defined(SSE_DRIVE)
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
    if(ADAT)
#endif
      SF314[DRIVE].State.motor=true;
    TRACE_LOG("Motor on drive %c\n",'A'+DRIVE);
#endif
    if (floppy_instant_sector_access){
      agenda_add(agenda_fdc_spun_up,MILLISECONDS_TO_HBLS(100),delay_exec);
    }else{
#if defined(SSE_FDC_SPIN_UP)
      ASSERT(ADAT);
      //  Set up agenda for next IP
      WD1772.IndexCounter=0;
      DWORD delay=SF314[DRIVE].HblsNextIndex();
      agenda_add(agenda_fdc_spun_up,delay,delay_exec);
#else
      // 6 revolutions but guaranteed 1 second spinup at 5 RPS, how?
      agenda_add(agenda_fdc_spun_up,MILLISECONDS_TO_HBLS(1100),delay_exec);
#endif
    }
  }
#if defined(SSE_DRIVE)
  else
    SF314[DRIVE].State.motor=true;
#endif

#if defined(SSE_FDC_394)
  }//if(num_connected_floppies!=DRIVE)
#endif

  if (delay_exec==0) fdc_execute();
}
//---------------------------------------------------------------------------
void agenda_fdc_spun_up(int do_exec)
{
#if defined(SSE_FDC_SPIN_UP)
/*  
On the WD1772 all commands, except the Force Interrupt Command,
 are programmed via the h Flag to delay for spindle motor start 
 up time. If the h Flag is not set and the MO signal is low when 
 a command is received, the WD1772 forces MO to a logic 1 and 
 waits 6 revolutions before executing the command. 
 At 300 RPM, this guarantees a one second spindle start up time.
 ->
 We count IP, only if there's a spinning selected drive
 Not emulated: if program changes drive and the new drive is spinning
 too, but IP occurs at different time!
*/
  if(ADAT)
  {
    ASSERT(WD1772.IndexCounter<6);
    //ASSERT(fdc_spinning_up);
    if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE&&SF314[DRIVE].State.motor) //TODO wd's line
      WD1772.IndexCounter++;
    if(WD1772.IndexCounter<6) // not finished
    {
      agenda_add(agenda_fdc_spun_up,FDC_HBLS_PER_ROTATION,do_exec);
      return;
    }
  }
#endif
  fdc_spinning_up=0;
  TRACE_LOG("FDC Drive spun exec %d\n",do_exec);
  if (do_exec) fdc_execute();
}
//---------------------------------------------------------------------------
void fdc_execute()
{
  // We need to do something here to make the command take more time
  // if the disk spinning up (fdc_spinning_up).

  int floppyno=floppy_current_drive();
//  ASSERT(!floppyno);
  TFloppyImage *floppy=&FloppyDrive[floppyno];
//  ASSERT(!(floppy->Empty() && floppy_head_track[floppyno]!=0));
  floppy_irq_flag=FLOPPY_IRQ_YES;
#if defined(SSE_FDC_ACCURATE_TIMING)
  int hbls_to_interrupt=64;
#define hbl_multiply 1
#else
  double hbls_to_interrupt=64.0;
  // The FDC timings don't change when you switch to mono, but HBLs do.
  // This variable corrects for that.
  double hbl_multiply=1.0;
  if (shifter_freq==MONO_HZ){
    hbl_multiply=double(HBLS_PER_SECOND_MONO)/double(HBLS_PER_SECOND_AVE);
  }
#endif

/*
The 177x accepts 11 commands.  Western Digital divides these commands
into four categories, labeled I,II, III, and IV.

COMMAND SUMMARY
     +------+----------+-------------------------+
     !	    !	       !	   BITS 	 !
     ! TYPE ! COMMAND  !  7  6	5  4  3  2  1  0 !
     +------+----------+-------------------------+
     !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
     !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
     !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
     !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
     !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
     !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
     !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
     !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
     !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
     !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
     !	 4  ! Forc int !  1  1	0  1 i3 i2 i1 i0 !
     +------+----------+-------------------------+

*/

  if ((fdc_cr & BIT_7)==0){
    // Type 1 commands
/*
Type I commands are Restore, Seek, Step, Step In, and Step Out.

The following table is a bit map of the values to store in the Command
Register.
Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
--------     -----     --     --     --     --     --     --     -----
Restore      0         0      0      0      h      V      r1     r0
Seek         0         0      0      1      h      V      r1     r0
Step         0         0      1      u      h      V      r1     r0
Step in      0         1      0      u      h      V      r1     r0
Step out     0         1      1      u      h      V      r1     r0

Flags:

u (Update Track Register) - If this flag is set, the 177x will update
the track register after executing the command.  If this flag is
cleared, the 177x will not update the track register.

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one-second start time. If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

V (Verify) - If this flag is set, the head settles after command
execution.  The settling time is 15 000 cycles for the 1772 and 30 000
cycles for the 1770.  The FDDC will then verify the track position of
the head.  The 177x reads the first ID field it finds and compares the
track number in that ID field against the Track Register.  If the
track numbers match but the ID field CRC is invalid, the 177x sets the
CRC Error bit in the status register and reads the next ID field.  If
the 177x does not find a sector with valid track number AND valid CRC
within 5 disk rotations, the chip sets the Seek Error bit in the
status register.

r (Step Time) - This bit pair determines the time between track steps
according to the following table:

r1       r0            1770                                        1772
--       --            ----                                        ----
0        0             6000 CPU clock cycles                       6000 cycles
0        1             12000 cycles                                12000 cycles
1        0             20 000 cycles                               2000 cycles
1        1             30 000 cycles                               3000 cycles

  SS 
  ST: 1772 r1 r0 11 -> 3000 cycles
  commands starting with 1 = seek  h  V r1 r0

13                                 0  0  1  1  seek
17                                 0  1  1  1  seek with Verify

53  = step in with update track register

  in fdc.h, we have this table that seems accurate:
  int fdc_step_time_to_hbls[4]={94,188,32,47};

*/
#if defined(SSE_WD1772) && defined(SSE_DEBUG)
    ASSERT(WD1772.CommandType(fdc_cr)==1);
#endif
    hbls_to_interrupt=fdc_step_time_to_hbls[fdc_cr & (BIT_0 | BIT_1)];
    // there's no stupid assert
    ASSERT( hbls_to_interrupt==94 || hbls_to_interrupt==188
      || hbls_to_interrupt==32 || hbls_to_interrupt==47 ); 
    switch (fdc_cr & (BIT_7+BIT_6+BIT_5+BIT_4)){
/*
Restore:
If the FDDC receives this command when the drive head is at track
zero, the chip sets its Track Register to $00 and ends the command.
If the head is not at track zero, the FDDC steps the head carriage
until the head arrives at track 0.  The 177x then sets its Track
Register to $00 and ends the command.  If the chip's track-zero input
does not activate after 255 step pulses AND the V bit is set in the
command word, the 177x sets the Seek Error bit in the status register
and ends the command.
*/
      case 0x00: //restore to track 0
//        TRACE_LOG("Restore (TR %d CYL %d)\n",fdc_tr,floppy_head_track[floppyno]);
        if (FDC_VERIFY && floppy->Empty()){ //no disk
          fdc_str=FDC_STR_SEEK_ERROR | FDC_STR_MOTOR_ON | FDC_STR_BUSY;
        }else{
#if !defined(SSE_FDC_394) // unused var
          if (floppy_head_track[floppyno]==0){
#if defined(SSE_FDC_ACCURATE_TIMING)
            hbls_to_interrupt=(ADAT)?1:2;
#else
            hbls_to_interrupt=2;
#endif
          }else{
            if (floppy_instant_sector_access==0) 
              hbls_to_interrupt*=floppy_head_track[floppyno];
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
            if(!ADAT)
#endif            
            floppy_head_track[floppyno]=0;
#endif
          }
#endif//#if !defined(SSE_FDC_394)
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*
If the head is not at track zero, the FDDC steps the head carriage
until the head arrives at track 0.  The 177x then sets its Track
Register to $00 and ends the command.  If the chip's track-zero input
does not activate after 255 step pulses AND the V bit is set in the
command word, the 177x sets the Seek Error bit in the status register
and ends the command.
*/
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
          if(true)
#else
          if(ADAT)
#endif
          {
            fdc_tr=255,fdc_dr=0; // like in CAPSimg
            floppy_irq_flag=0;
#if defined(SSE_BUGFIX_394)
            if(floppy_head_track[floppyno]==0 && DRIVE!=num_connected_floppies)
              fdc_tr=0; // Overdrive fast
#endif
            agenda_add(agenda_floppy_seek,1,0); //1 scanline
          }
          else
#endif
          fdc_tr=0;
          fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
          floppy_type1_command_active=1;
          dbg_log(Str("FDC: Restored drive ")+char('A'+floppyno)+" to track 0");
        }
        break;
/*
Seek:
The CPU must load the desired track number into the Data Register
before issuing this command.  The Seek command causes the 177x to step
the head to the desired track number and update the Track Register.

*/
      case 0x10: //seek to track number in data register
//        TRACE_LOG("Seek %d (TR %d CYL %d)\n",fdc_dr,fdc_tr,floppy_head_track[floppyno]);
        agenda_add(agenda_floppy_seek,2,0);
        fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
        floppy_irq_flag=0;
        dbg_log(Str("FDC: Seeking drive ")+char('A'+floppyno)+" to track "+fdc_dr+" hbl_count="+hbl_count);
        floppy_type1_command_active=1;
        break;

/*
Step:
The 177x issues one step pulse to the mechanism, then delays one step
time according to the r flag.

Step in:
The 177x issues one step pulse in the direction toward Track 76 and
waits one step time according to the r flag.  [Transcriber's Note:
Western Digital assumes in this paragraph that disks do not have more
than 77 tracks.]

Step out:
The 177x issues one step pulse in the direction toward Track 0 and
waits one step time according to the r flag.

The chip steps the drive head in the same direction it last stepped
unless the command changes the direction.  Each step pulse takes 4
cycles.  The 177x begins outputting a direction signal to the drive 24
cycles before the first stepping pulse.
*/
      default: //step, step in, step out
      {
#if defined(SSE_DRIVE_SOUND)
        if(OPTION_DRIVE_SOUND
#if defined(SSE_FDC_393A)
          && YM2149.Drive()!=TYM2149::NO_VALID_DRIVE
#endif
          )
          SF314[DRIVE].Sound_Step();
#endif
        fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
        char d=1; //step direction, default is inwards
        if (floppy->Empty()
#if defined(SSE_FDC_393A)
          || YM2149.Drive()==TYM2149::NO_VALID_DRIVE //Japtro
#endif          
          ){
          if (FDC_VERIFY) fdc_str|=FDC_STR_SEEK_ERROR;
        }else{
          switch (fdc_cr & (BIT_7+BIT_6+BIT_5)){
            case 0x20: if (fdc_last_step_inwards_flag==0) d=-1; break;
            case 0x60: d=-1; break;
          }
          fdc_last_step_inwards_flag = (d==1);
          if (fdc_cr & BIT_4){ //U flag, update track register
            fdc_tr+=d;
          }
          if (d==-1 && floppy_head_track[floppyno]==0){   //trying to step out from track 0
            fdc_tr=0; //here we set the track register
          }else{ //can step
            floppy_head_track[floppyno]+=d;
            dbg_log(Str("FDC: Stepped drive ")+char('A'+floppyno)+" to track "+floppy_head_track[floppyno]);
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
            if(ADAT)
              floppy_irq_flag=0;// IRQ set in fdc_type1_check_verify()
#endif
            fdc_type1_check_verify();
          }
          floppy_type1_command_active=1;
//          TRACE_LOG("Step %d (TR %d CYL %d)\n",d,fdc_tr,floppy_head_track[floppyno]);
        }
      }
    }
  }else{
    floppy_type1_command_active=0;
    fdc_str&=BYTE(~FDC_STR_WRITE_PROTECT);
    LOG_ONLY( int n_sectors=dma_sector_count; )
    switch (fdc_cr & (BIT_7+BIT_6+BIT_5+BIT_4)){

/*
Type II commands are Read Sector and Write Sector.

Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
------------     -----     --     --     --     --     --     --     -----
Read Sector      1         0      0      m      h      E      0      0
Write Sector     1         0      1      m      h      E      P      a0

Flags:

m (Multiple Sectors) - If this bit = 0, the 177x reads or writes
("accesses") only one sector.  If this bit = 1, the 177x sequentially
accesses sectors up to and including the last sector on the track.  A
multiple-sector command will end prematurely when the CPU loads a
Force Interrupt command into the Command Register.

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one- second start time.  If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

E (Settling Delay) - If this flag is set, the head settles before
command execution.  The settling time is 15 000 cycles for the 1772
and 30 000 cycles for the 1770.

P (Write Precompensation) - On the 1770-02 and 1772-00, a 0 value in
this bit enables automatic write precompensation.  The FDDC delays or
advances the write bit stream by one-eighth of a cycle according to
the following table.

Previous          Current bit           Next bit
bits sent         sending               to be sent       Precompensation
---------         -----------           ----------       ---------------
x       1         1                     0                Early
x       0         1                     1                Late
0       0         0                     1                Early
1       0         0                     0                Late

Programmers typically enable precompensation on the innermost tracks,
where bit shifts usually occur and bit density is maximal.  A 1 value
for this flag disables write precompensation.

a0 (Data Address Mark) - If this bit is 0, the 177x will write a
normal data mark.  If this bit is 1, the 177x will write a deleted
data mark.

Read Sector:
The controller waits for a sector ID field that has the correct track
number, sector number, and CRC.  The controller then checks for the
Data Address Mark, which consists of 43 copies of the second byte of
the CRC.  If the controller does not find a sector with correct ID
field and address mark within 5 disk revolutions, the command ends.
Once the 177x finds the desired sector, it loads the bytes of that
sector into the data register.  If there is a CRC error at the end of
the data field, the 177x sets the CRC Error bit in the Status Register
and ends the command regardless of the state of the "m" flag.

Write Sector:
The 177x waits for a sector ID field with the correct track number,
sector number, and CRC.  The 177x then counts off 22 bytes from the
CRC field.  If the CPU has not loaded a byte into the Data Register
before the end of this 22-byte delay, the 177x ends the command.
Assuming that the CPU has heeded the 177x's data request, the
controller writes 12 bytes of zeroes.  The 177x then writes a normal
or deleted Data Address Mark according to the a0 flag of the command.
Next, the 177x writes the byte which the CPU placed in the Data
Register, and continues to request and write data bytes until the end
of the sector.  After the 177x writes the last byte, it calculates and
writes the 16-bit CRC.  The chip then writes one $ff byte.  The 177x
interrupts the CPU 24 cycles after it writes the second byte of the
CRC.
*/

      // Type 2
      case 0x80:case 0xa0:LOG_ONLY( n_sectors=1; ) // Read/write single sector
      case 0x90:case 0xb0:                         // Read/write multiple sectors

#if defined(SSE_WD1772) && defined(SSE_DEBUG)
        ASSERT(WD1772.CommandType(fdc_cr)==2);
#endif

        if (floppy->Empty() || floppy_head_track[floppyno]>FLOPPY_MAX_TRACK_NUM
#if defined(SSE_FDC_394)
          || (floppyno==num_connected_floppies) //no drive
#endif          
          ){
          TRACE_LOG("Drive empty or track %d overshoot\n",floppy_head_track[floppyno]);
#if defined(SSE_FDC_393B)
          fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
#else
          fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;
#endif
          floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
          break;
        }
#ifdef ENABLE_LOGFILE
        {
          Str RW="Reading",Secs="one sector";
          if (fdc_cr & 0x20) RW="Writing";
          if (n_sectors>1) Secs="multiple sectors";
          dbg_log(Str("FDC: ")+RW+" "+Secs+" from drive "+char('A'+floppyno)+
                 " track "+floppy_head_track[floppyno]+
                 " side "+floppy_current_side()+
                 " sector "+fdc_sr+
                 " into address "+HEXSl(dma_address,6)+
                 " dma_sector_count="+dma_sector_count);
        }
#endif
        if (floppy_instant_sector_access){

          agenda_add(agenda_floppy_readwrite_sector,int(hbls_to_interrupt*hbl_multiply),MAKELONG(0,fdc_cr));

          fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
          floppy_irq_flag=0;
        }else{
          FDC_IDField IDList[30];
          int SectorIdx=-1;
          //this should work with STW too as this has been modded:
          int nSects=floppy->GetIDFields(floppy_current_side(),floppy_head_track[floppyno],IDList);
          for (int n=0;n<nSects;n++){
            if (IDList[n].Track==fdc_tr 
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
#elif defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*  Side in ID field may have whatever value. The WD1772 chip doesn't control
    side. In the ST, it is done by the YM2149 PSG.
*/
              && (ADAT||IDList[n].Side==floppy_current_side())
#else
              && IDList[n].Side==floppy_current_side()
#endif
              ){
              if (IDList[n].SectorNum==fdc_sr){
                SectorIdx=n;
                break;
              }
            }
          }
#if defined(SSE_FDC_ACCURATE_TIMING)
          if (SectorIdx>-1 && nSects>0) {
            BYTE num=fdc_sr;
            WORD CurrentByte=SF314[DRIVE].BytePosition();
            WORD BytesToNextID=Disk[DRIVE].BytesToID(num);
            //TRACE_FDC("Current %d Start ID %d in %d, at %d ",CurrentByte,num,BytesToNextID,BytesToNextID+CurrentByte);
            // 12+3: to ID; 16: will be counted in agenda function
            BYTE pre_data_gap=Disk[DRIVE].PreDataGap()-12-3-16; 
            //TRACE_FDC("gap %d\n",pre_data_gap);
            WORD start=(CurrentByte+BytesToNextID+pre_data_gap)%Disk[DRIVE].TrackBytes;
            DWORD HBLOfSectorStart=hbl_count+SF314[DRIVE].BytesToHbls(BytesToNextID+pre_data_gap);
            if(WD1772.CR&BIT_2)
              HBLOfSectorStart+=MILLISECONDS_TO_HBLS(15);//?
            //TRACE_FDC("hbl now %d then %d, diff %d\n",hbl_count,HBLOfSectorStart,HBLOfSectorStart-hbl_count);
            HBLOfSectorStart-=2;//see below, Steem's way
#else //Steem 3.2
          if (SectorIdx>-1){
            // Break up the readable track into nSects sections,
            // agenda_floppy_readwrite_sector occurs at the start of one of these sections
            DWORD HBLsPerTrack=FDC_HBLS_PER_ROTATION-FDC_HBLS_OF_INDEX_PULSE;
            DWORD HBLsPerSector=HBLsPerTrack/nSects;
            DWORD HBLsAtStartOfRotation=(hbl_count/FDC_HBLS_PER_ROTATION)*FDC_HBLS_PER_ROTATION;
            DWORD HBLOfSectorStart=HBLsAtStartOfRotation + SectorIdx*HBLsPerSector;
            if (HBLOfSectorStart<hbl_count) HBLOfSectorStart+=FDC_HBLS_PER_ROTATION;
#endif

            agenda_delete(agenda_floppy_readwrite_sector);
#if defined(SSE_FDC_ACCURATE_TIMING)
            agenda_add(agenda_floppy_readwrite_sector,int(hbl_multiply
              *(HBLOfSectorStart-hbl_count))+2,MAKELONG(0,start));
#else
            agenda_add(agenda_floppy_readwrite_sector,int(hbl_multiply*(HBLOfSectorStart-hbl_count))+2,MAKELONG(0,fdc_cr));
#endif
            floppy_irq_flag=0;
            fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;

          }else{
            TRACE_LOG("SectorIdx %d nSects %d\n",SectorIdx,nSects);
            floppy_irq_flag=FLOPPY_IRQ_ONESEC;
#if defined(SSE_FDC_393B)
            fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
#else
            fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;  //sector not found
#endif
          }

        }
        break;

      // Type 3
/*
Type III commands are Read Address, Read Track, and Write Track.

Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
------------     -----     --     --     --     --     --     --     -----
Read Address     1         1      0      0      h      E      0      0      C0
Read Track       1         1      1      0      h      E      0      0      E0
Write Track      1         1      1      1      h      E      P      0      F0

Flags:

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one- second start time.  If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

E (Settling Delay) - If this flag is set, the head settles before
command execution.  The settling time is 15 000 cycles for the 1772
and 30 000 cycles for the 1770.

P (Write Precompensation) - On the 1770-02 and 1772-00, a 0 value in
this bit enables automatic write precompensation.  The FDDC delays or
advances the write bit stream by one-eighth of a cycle according to
the following table.

Previous          Current bit           Next bit
bits sent         sending               to be sent       Precompensation
---------         -----------           ----------       ---------------
x       1         1                     0                Early
x       0         1                     1                Late
0       0         0                     1                Early
1       0         0                     0                Late

Programmers typically enable precompensation on the innermost tracks,
where bit shifts usually occur and bit density is maximal.  A 1 value
for this flag disables write precompensation.
*/
      case 0xc0: //read address
/*
Read Address:
The 177x reads the next ID field it finds, then sends the CPU the
following six bytes via the Data Register:
Byte #     Meaning                |     Sector length code     Sector length
------     ------------------     |     ------------------     -------------
1          Track                  |     0                      128
2          Side                   |     1                      256
3          Sector                 |     2                      512
4          Sector length code     |     3                      1024
5          CRC byte 1             |
6          CRC byte 2             |

[Transcriber's Note:  | is the vertical bar character.]

The 177x copies the track address into the Sector Register.  The chip
sets the CRC Error bit in the status register if the CRC is invalid.
*/
#if defined(SSE_WD1772) && defined(SSE_DEBUG)
        ASSERT(WD1772.CommandType(fdc_cr)==3);
#endif
        dbg_log(Str("FDC: Type III Command - read address to ")+HEXSl(dma_address,6)+"from drive "+char('A'+floppyno));

        if (floppy->Empty()){
          floppy_irq_flag=0;  //never cause interrupt, timeout
        }else{
          FDC_IDField IDList[30];
          DWORD nSects=floppy->GetIDFields(floppy_current_side(),floppy_head_track[floppyno],IDList);

          if (nSects
#if defined(SSE_DRIVE_SINGLE_SIDE_RND)
            &&!(SSEOption.SingleSideDriveMap&(DRIVE+1)&&CURRENT_SIDE==1)
#endif
            ){
#if defined(SSE_DISK_READ_ADDRESS_TIMING)
/*  Using a more precise routine for timings, this fixes ProCopy 'Analyze'
*/
            WORD HBLsToNextSector=0;
            BYTE NextIDNum; // it's index 0 - #sectors-1
            Disk[DRIVE].NextID(NextIDNum,HBLsToNextSector); //references
            HBLsToNextSector-=2; //see below
#else
            DWORD DiskPosition=hbl_count % FDC_HBLS_PER_ROTATION;
            // Break up the track into nSects sections, agenda_read_address
            // occurs at the end of one of these sections
            DWORD HBLsPerTrack=FDC_HBLS_PER_ROTATION-FDC_HBLS_OF_INDEX_PULSE;
            DWORD HBLsPerSector=HBLsPerTrack/nSects;

            DWORD HBLsToNextSector=HBLsPerSector - (DiskPosition % HBLsPerSector);
            DWORD NextIDNum=DiskPosition / HBLsPerSector;

            if (NextIDNum>=nSects){ // Track index pulse
              NextIDNum=0;
              // Go to next revolution
              HBLsToNextSector=(FDC_HBLS_PER_ROTATION-DiskPosition) + HBLsPerSector;
            }
#endif
            agenda_delete(agenda_floppy_read_address);
            agenda_add(agenda_floppy_read_address,int(hbl_multiply*HBLsToNextSector)+2,NextIDNum);
            floppy_irq_flag=0;
            fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
          }else{
            fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;  //sector not found
            floppy_irq_flag=0;  //never cause interrupt, timeout
          }
        }
        break;
/*
Read Track:
This command dumps a raw track, including gaps, ID fields, and data,
into the Data Register.  The FDDC starts reading with the leading edge
of the first index pulse it finds, and stops reading with the next
index pulse.  During this command, the FDDC does not check CRCs.  The
address mark detector is on during the entire command.  (The address
mark detector detects ID, data and index address marks during read and
write operations.)  Because the address mark detector is always on,
write splices or noise may cause the chip to look for an address mark.
[Transcriber's Note: I do not know how the programmer can tell that
the AM detector has found an address mark.]  The chip may read gap
bytes incorrectly during write-splice time because of synchronization.
*/
      case 0xe0:  //read track
#if defined(SSE_WD1772) && defined(SSE_DEBUG)
        ASSERT(WD1772.CommandType(fdc_cr)==3);
#endif
        dbg_log(Str("FDC: Type III Command - read track to ")+HEXSl(dma_address,6)+" from drive "+char('A'+floppyno)+
                  " dma_sector_count="+dma_sector_count);

        fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
        floppy_irq_flag=0;
        if (floppy->DiskInDrive()){
          agenda_delete(agenda_floppy_read_track);
        DWORD DiskPosition=hbl_count % FDC_HBLS_PER_ROTATION;
        agenda_add(agenda_floppy_read_track,int(hbl_multiply*(FDC_HBLS_PER_ROTATION-DiskPosition)),0);
        }
        break;

/*
Write Track:
This command is the means of formatting disks.  The drive head must be
over the correct track BEFORE the CPU issues the Write Track command.
Writing starts with the leading edge of the first index pulse which
the 177x finds.  The 177x stops writing when it encounters the next
index pulse.  The 177x sets the Data Request bit immediately after
receiving the Write Track command, but does not start writing until
the CPU loads the Data Register.  If the CPU does not send the 177x a
byte within three byte times after the first index pulse, the 177x
ends the command.  The 177x will write all data values from $00 to $f4
(inclusive) and from $f8 to $ff (inclusive) unaltered.  Data values
$f5, $f6, and $f7, however, have special meanings.  The value $f5
means to write an $a1 to the disk.  The $a1 value which the 177x
writes to the disk will lack an MFM clock transition between bits 4
and 5.  This missing clock transition indicates that the next normally
written byte will be an address mark.  In addition, a Data Register
value of $f5 will reset the 177x's CRC generator.  A Data Register
value of $f6 will not reset the CRC generator but will write a pre-
address-mark value of $c2 to the disk.  The written $c2 will lack an
MFM clock transition between bits 3 and 4.  A Data Register value of
$f7 will write a two-byte CRC to the disk.
*/
      case 0xf0:  //write (format) track
#if defined(SSE_WD1772) && defined(SSE_DEBUG)
        ASSERT(WD1772.CommandType(fdc_cr)==3);
#endif
        dbg_log(Str("FDC: - Type III Command - write track from address ")+HEXSl(dma_address,6)+" to drive "+char('A'+floppyno));

        floppy_irq_flag=0;
        fdc_str=FDC_STR_MOTOR_ON | FDC_STR_BUSY;
        if (floppy->DiskInDrive() && floppy->ReadOnly==0 && floppy_head_track[floppyno]<=FLOPPY_MAX_TRACK_NUM){
          if (floppy->Format_f==NULL) floppy->OpenFormatFile();
          if (floppy->Format_f){
            agenda_delete(agenda_floppy_write_track);
            DWORD DiskPosition=hbl_count % FDC_HBLS_PER_ROTATION;
            agenda_add(agenda_floppy_write_track,int(hbl_multiply*(FDC_HBLS_PER_ROTATION-DiskPosition)),0);
            floppy_write_track_bytes_done=0;
          }
        }
        break;
/*
The Type IV command is Force Interrupt.

Force Interrupt:
Programmers use this command to stop a multiple-sector read or write
command or to ensure Type I status in the Status Register.  The format
of this command is %1101(I3)(I2)00.  If flag I2 is set, the 177x will
acknowledge the command at the next index pulse.  If flag I3 is set,
the 177x will immediately stop what it is doing and generate an
interrupt.  If neither I2 nor I3 are set, the 177x will not interrupt
the CPU, but will immediately stop any command in progress.  After the
CPU issues an immediate interrupt command ($d8), it MUST write $d0
(Force Interrupt, I2 clear, I3 clear) to the Command Register in order
to shut off the 177x's interrupt output.  After any Force Interrupt
command, the CPU must wait 16 cycles before issuing any other command.
If the CPU does not wait, the 177x will ignore the previous Force
Interrupt command.  Because the 177x is microcoded, it will
acknowledge Force Interrupt commands only between micro- instructions.
*/
      case 0xd0:        //force interrupt
      {
#if defined(SSE_WD1772) && defined(SSE_DEBUG)
        ASSERT(WD1772.CommandType(fdc_cr)==4);
//        TRACE_LOG("Force Interrupt\n");
#endif
        dbg_log(Str("FDC: ")+HEXSl(old_pc,6)+" - Force interrupt: t="+hbl_count);

#if defined(SSE_FDC_393C)
/*  On the common $90-$D0 sequence, the command is still active when it is 
    interrupted, and STR isn't 'type I'. 
*/
        if(!(fdc_str&FDC_STR_BUSY))
          floppy_type1_command_active=2; // PYM/BPOC doesn't like 'track0' 
#else
        bool type23_active=(agenda_get_queue_pos(agenda_floppy_readwrite_sector)>=0 ||
                            agenda_get_queue_pos(agenda_floppy_read_address)>=0 ||
                            agenda_get_queue_pos(agenda_floppy_read_track)>=0 ||
                            agenda_get_queue_pos(agenda_floppy_write_track)>=0);
#endif
        agenda_delete(agenda_floppy_seek);
        agenda_delete(agenda_floppy_readwrite_sector);
        agenda_delete(agenda_floppy_read_address);
        agenda_delete(agenda_floppy_read_track);
        agenda_delete(agenda_floppy_write_track);
#if defined(SSE_FDC_ACCURATE)
        if(
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
          true
#else
          ADAT
#endif
          )
        {
#if defined(SSE_FDC_VERIFY_AGENDA)
          agenda_delete(agenda_fdc_verify);
#endif
#if defined(SSE_FDC_SPIN_UP)
          agenda_delete(agenda_fdc_spun_up); // Holocaust/Blood real start
#endif
        }
#endif
        agenda_delete(agenda_fdc_finished);
        fdc_str=BYTE(fdc_str & FDC_STR_MOTOR_ON);
        if (fdc_cr & b1100){
#if defined(SSE_FDC_FORCE_INTERRUPT)
/*  "The lower four bits of the command determine the conditional 
interrupt as follows:
- i0,i1 = Not used with the WD1772
- i2 = Every Index Pulse
- i3 = Immediate Interrupt
The conditional interrupt is enabled when the corresponding bit 
positions of the command (i3-i0) are set to a 1. When the 
condition for interrupt is met the INTRQ line goes high signifying
 that the condition specified has occurred. If i3-i0 are all set
 to zero (Hex D0), no interrupt occurs but any command presently
 under execution is immediately terminated. When using the immediate
 interrupt condition (i3 = 1) an interrupt is immediately generated
 and the current command terminated. Reading the status or writing
 to the Command Register does not automatically clear the interrupt.
 The Hex D0 is the only command that enables the immediate interrupt
 (Hex D8) to clear on a subsequent load Command Register or Read 
 Status Register operation. Follow a Hex D8 with D0 command."

  -> D4 (IRQ every index pulse) wasn't implemented yet in Steem. 
  Done in v3.5.3. Fixes Panzer rotation time returning null times.
*/          
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
          if(true)
#else
          if(ADAT)
#endif
          {
            if(fdc_cr&b1000)
            {
              ASSERT( fdc_cr==0xD8 );
              WD1772.InterruptCondition=8;
              //TRACE_LOG("Immediate IRQ\n");
              agenda_fdc_finished(0); // Interrupt CPU immediately
            }
            else 
            {
              ASSERT( fdc_cr==0xD4 );
              WD1772.InterruptCondition=4;// IRQ at every index pulse
              floppy_irq_flag=false;
              WORD hbls_to_next_ip=SF314[DRIVE].HblsNextIndex();
              //TRACE_LOG("IP IRQ in %d HBL (%d)\n",hbls_to_next_ip,hbl_count+hbls_to_next_ip);
              agenda_add(agenda_fdc_finished,hbls_to_next_ip,0);
            }
          }
          else
#endif
            agenda_fdc_finished(0); // Interrupt CPU immediately
        }else{ //SS D0
          ASSERT( fdc_cr==0xD0 );
          floppy_irq_flag=0;
#if defined(SSE_FDC_FORCE_INTERRUPT)
          WD1772.InterruptCondition=0;
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
          if(true)
#else
          if(ADAT)
#endif
            agenda_add(agenda_fdc_motor_flag_off,FDC_HBLS_PER_ROTATION*9,0); //no IP ctr...
          else
#endif
          mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
        }
        /* From Jorge Cwik
          By the way, the status after a Type IV command depends on the previous
          state of the fdc. If there was a command active, the status is the type of
          the previous command. If there was no command, then the status is of
          Type I. So resetting the FDC twice in a row guarantee a Type I status
          (first time terminates a command, if one is active; second time activates
          a type I status because no command was active).
        */
#if !defined(SSE_FDC_393C) //see above
        if (type23_active){
          floppy_type1_command_active=0;
        }else{
          floppy_type1_command_active=2;
        }
#endif
        break;
      }
    }
  }
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
  if(!ADAT)
#endif
  if (fdc_str & FDC_STR_MOTOR_ON) agenda_add(agenda_fdc_motor_flag_off,MILLISECONDS_TO_HBLS(1800),0);

  if (floppy_irq_flag){
    if (floppy_irq_flag!=FLOPPY_IRQ_NOW){ // Don't need to add agenda if it has happened
      if (floppy_irq_flag==FLOPPY_IRQ_ONESEC){
#if defined(SSE_FDC_ACCURATE_TIMING)
        TRACE_LOG("Error - 5REV - IRQ in %d HBLs\n",FDC_HBLS_PER_ROTATION*5);
        agenda_add(agenda_fdc_finished,FDC_HBLS_PER_ROTATION*5,0);
#else
        TRACE_LOG("1SEC - IRQ in %d HBLs\n",MILLISECONDS_TO_HBLS(1000));
        agenda_add(agenda_fdc_finished,MILLISECONDS_TO_HBLS(1000),0);
#endif
      }else{
//        TRACE_LOG("IRQ in %d HBLs\n",int(hbls_to_interrupt*hbl_multiply));
        agenda_add(agenda_fdc_finished,int(hbls_to_interrupt*hbl_multiply),0);
      }
    }
  }

}
//---------------------------------------------------------------------------
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)

void agenda_fdc_motor_flag_off(int revs_to_wait)
{
/*
 "If after finishing the command, the device remains idle for
 9 revolutions, the MO signal goes back to a logic 0."
  ->
  To ensure 9 revs, the controller counts 10 IP.
  We do the same as before, using our new variable IndexCounter
  We don't use 'revs_to_wait'

  We count IP, only if there's a spinning selected drive
  Not emulated: if program changes drive and the new drive is spinning
  too, but IP occurs at different time!

  Cases Symic; Treasure Trap -FOF

*/

  if(ADAT)
  {
    if(YM2149.Drive()!=TYM2149::NO_VALID_DRIVE && SF314[DRIVE].State.motor
#if defined(SSE_DRIVE_EMPTY_VERIFY_TIME_OUT2)
      &&!FloppyDrive[DRIVE].Empty()
#endif
      )
      WD1772.IndexCounter++;
    if(WD1772.IndexCounter<10)
    {
      agenda_add(agenda_fdc_motor_flag_off,FDC_HBLS_PER_ROTATION,revs_to_wait);
      return;
    }
    WD1772.IndexCounter=0; 
  }
  fdc_str&=BYTE(~FDC_STR_MOTOR_ON);

  TRACE_LOG("Motor off drive %c\n",'A'+DRIVE);
  SF314[DRIVE].State.motor=false;
}

#else

void agenda_fdc_motor_flag_off(int)
{
  fdc_str&=BYTE(~FDC_STR_MOTOR_ON);
}

#endif
//---------------------------------------------------------------------------

#if defined(SSE_FDC_VERIFY_AGENDA)

void agenda_fdc_verify(int) {
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
  ASSERT( ADAT );
#endif
  if(floppy_type1_command_active && FDC_VERIFY)
  {
//    TRACE_LOG("Verify\n");
    // This reads an ID field and checks that track number matches floppy_head_track
    // It will fail on an unformatted track or if there is no disk of course
    int floppyno=floppy_current_drive();
    TFloppyImage *floppy=&FloppyDrive[floppyno];
    if (floppy_head_track[floppyno]>FLOPPY_MAX_TRACK_NUM){
      fdc_str|=FDC_STR_SEEK_ERROR;
    }
    else if(floppy_head_track[floppyno]!=fdc_tr) // track not correct
    {
      fdc_str|=FDC_STR_SEEK_ERROR;
    }
    else if (floppy->TrackIsFormatted[floppy_current_side()][floppy_head_track[floppyno]]==0){
      // If track is formatted then it is okay to seek to it, otherwise do this:
      if (floppy_head_track[floppyno]>=floppy->TracksPerSide) fdc_str|=FDC_STR_SEEK_ERROR;
      if (floppy_current_side() >= floppy->Sides) fdc_str|=FDC_STR_SEEK_ERROR;
    }
    DEBUG_ONLY( if (fdc_str & FDC_STR_SEEK_ERROR) dbg_log("     Verify failed (track not formatted)"); )
#if defined(SSE_DEBUG)
    if(fdc_str & FDC_STR_SEEK_ERROR)
      TRACE_LOG("Verify error TR %d CYL %d\n",fdc_tr,floppy_head_track[floppyno]);
#endif
  }
  agenda_fdc_finished(0);
}

#endif

//---------------------------------------------------------------------------
void agenda_fdc_finished(int)
{
#if defined(SSE_DMA) && defined(SSE_DEBUG)
  if(TRACE_ENABLED(LOGSECTION_FDC)) 
    Dma.UpdateRegs(true); // -> "IRQ" trace (including 'busy')
#endif

#if defined(SSE_DRIVE_SOUND) 
  if(OPTION_DRIVE_SOUND && (!ADAT || OPTION_DRIVE_SOUND_SEEK_SAMPLE)) //TODO
    SF314[DRIVE].Sound_CheckIrq();
#endif//snd

  dbg_log("FDC: Finished command, GPIP bit low.");
#if defined(SSE_FDC_393B)
  if(floppy_irq_flag==FLOPPY_IRQ_ONESEC)
    fdc_str|=FDC_STR_SEEK_ERROR;
#endif
  floppy_irq_flag=FLOPPY_IRQ_NOW;
  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,0); // Sets bit in GPIP low (and it stays low)

  fdc_str&=BYTE(~FDC_STR_BUSY); // Turn off busy bit
  fdc_str&=BYTE(~FDC_STR_T1_TRACK_0); // This is lost data bit for non-type 1 commands
#if defined(SSE_DISK_GHOST)
  if(!(OPTION_GHOST_DISK && WD1772.Lines.CommandWasIntercepted)) // spurious Lost Data
#endif
  if (floppy_type1_command_active){
    if (floppy_head_track[floppy_current_drive()]==0) fdc_str|=FDC_STR_T1_TRACK_0;
    floppy_type1_command_active=2;
  }

#if defined(SSE_DISK_GHOST)
  if((OPTION_GHOST_DISK 
    && WD1772.Lines.CommandWasIntercepted))
    ioaccess|=IOACCESS_FLAG_FOR_CHECK_INTRS;
#endif

#if defined(SSE_FDC_FORCE_INTERRUPT)
/*  Command $D4 will trigger an IRQ at each index pulse.
    Motor keeps running.
    It's a way to measure rotation time (Panzer)
*/
  if(
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)    
    ADAT && 
#endif
    WD1772.InterruptCondition==4)
  {
//    TRACE_LOG("D4 prepare next IP at %d\n",hbl_count+FDC_HBLS_PER_ROTATION);
    agenda_add(agenda_fdc_finished,FDC_HBLS_PER_ROTATION,0);    
  } 
  else 
#endif
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
  if(ADAT)
  {
    //  Set up agenda for next IP
    agenda_delete(agenda_fdc_motor_flag_off); //3.6.2
    WD1772.IndexCounter=0;
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
    DWORD delay=(ADAT)?(SF314[DRIVE].HblsNextIndex()):2;
#else
    DWORD delay=SF314[DRIVE].HblsNextIndex();
#endif
    agenda_add(agenda_fdc_motor_flag_off,delay,0);
  }
#endif
}
//---------------------------------------------------------------------------
void agenda_floppy_seek(int)
{
  int floppyno=floppy_current_drive();
  //ASSERT(!floppyno);
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*
"SEEK
This command assumes that the track register contains the track number of the
current position of the Read/Write head and the Data Register contains the
desired track number. The WD1770 will update the Track Register and issue
stepping pulses in the appropiate direction until the contents of the Track
Register are equal to the contents of the Data Register (the desired Track
location). A verification operation takes place if the V flag is on. The h
bit allows the Motor On option at the start of the command. An interrupt is
generated at the completion of the command. Note: When using mutiple drives,
the track register must be updated for the drive selected before seeks are
issued."
  ->
  Seek works with DR and TR, not DR and disk track.
*/
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST) && !defined(SSE_BUGFIX_394)
  if(true)
#else
  if(ADAT)
#endif
  {
    if(fdc_tr==fdc_dr)
    {
      dbg_log(Str("FDC: Finished seeking to track ")+fdc_dr+" hbl_count="+hbl_count);
      fdc_type1_check_verify();
#if !defined(SSE_FDC_VERIFY_AGENDA)
      agenda_fdc_finished(0);
#endif
      return;
    }
    else 
    {
#if defined(SSE_DRIVE_SOUND)
      if(OPTION_DRIVE_SOUND && !OPTION_DRIVE_SOUND_SEEK_SAMPLE)
        SF314[DRIVE].Sound_Step();
#endif
      if(fdc_tr>fdc_dr)
      {
        fdc_tr--;

        if(floppy_head_track[floppyno])
          floppy_head_track[floppyno]--;    
        if(!floppy_head_track[floppyno]
#if defined(SSE_FDC_394)
          && floppyno!=num_connected_floppies
#endif
        )
        {
          if(!(fdc_cr&0xF0)) // condition?
            fdc_tr=0; // this is  how RESTORE works
          fdc_str|=FDC_STR_T1_TRACK_0;
        }
#if defined(SSE_FDC_394)
/*  
If the TR00* input does not go active low after 255 stepping pulses,
the WD1772 terminates operation, interrupts, and sets the Seek Error
status bit, providing the v flag is set.
->
If RESTORE fails to set TR00 in the status byte, TOS knows there's no
2nd drive.
*/
        else if(!(fdc_cr&0xF0) && !fdc_tr)
        {
          ASSERT(floppyno==num_connected_floppies); //1
          if(FDC_VERIFY)
            fdc_str|=FDC_STR_SEEK_ERROR;
          agenda_fdc_finished(0); //IRQ
        }
#endif
      }
      else
      {
        ASSERT( fdc_tr<fdc_dr );
        fdc_tr++;
        if(floppy_head_track[floppyno]<FLOPPY_MAX_TRACK_NUM)
          floppy_head_track[floppyno]++;
      }
    }
  }
  else // !ADAT or not defined
#endif
  {//SS scope
/*  In Steem 3.2, seek was directly using disk track instead of TR
    It still works that way in fast mode.
    It was already using an agenda for each step.
*/
    if (floppy_head_track[floppyno]==fdc_dr){
      dbg_log(Str("FDC: Finished seeking to track ")+fdc_dr+" hbl_count="+hbl_count);
      fdc_tr=fdc_dr;
      fdc_type1_check_verify();
      agenda_fdc_finished(0);
      return;
    }
    if (floppy_head_track[floppyno]>fdc_dr){
      floppy_head_track[floppyno]--;
    }else if (floppy_head_track[floppyno]<fdc_dr){
      floppy_head_track[floppyno]++;
    }
  }//SS scope
  int hbls_to_interrupt=fdc_step_time_to_hbls[fdc_cr & (BIT_0 | BIT_1)];
  if (floppy_instant_sector_access) hbls_to_interrupt>>=5;
#if defined(SSE_FDC_ACCURATE_TIMING)
  agenda_add(agenda_floppy_seek,hbls_to_interrupt,0);
#else
  double hbl_multiply=1.0;
  if (shifter_freq==MONO_HZ) hbl_multiply=double(HBLS_PER_SECOND_MONO)/double(HBLS_PER_SECOND_AVE);
  agenda_add(agenda_floppy_seek,int(hbl_multiply*double(hbls_to_interrupt)),0);
#endif
}
//---------------------------------------------------------------------------
void agenda_floppy_readwrite_sector(int Data)
{
  int floppyno=floppy_current_drive();
  ASSERT(floppyno==0 || floppyno==1);
  TFloppyImage *floppy=&FloppyDrive[floppyno];
  ASSERT(floppy);
  bool FromFormat=0;
  if (floppy_head_track[floppyno]<=FLOPPY_MAX_TRACK_NUM){
    FromFormat=floppy->TrackIsFormatted[floppy_current_side()][floppy_head_track[floppyno]];
  }
#if defined(SSE_FDC_ACCURATE_TIMING)//hbl drift?
  int Command=fdc_cr; //shouldn't change...
#else
  int Command=HIWORD(Data);
#endif
  BYTE WriteProtect=0;
  if ((Command & 0x20) && floppy->ReadOnly){ // Write
    WriteProtect=FDC_STR_WRITE_PROTECT;
  }
  if (floppy->Empty()){
    TRACE_LOG("No disk %c\n",floppyno+'A');
    fdc_str=BYTE(WriteProtect | FDC_STR_MOTOR_ON | /*FDC_STR_SEEK_ERROR | */FDC_STR_BUSY);
    agenda_add(agenda_fdc_finished,FDC_HBLS_PER_ROTATION*int((shifter_freq==MONO_HZ) ? 11:5),0);
    return; // Don't loop
  }
#if !defined(SSE_OSD_DRIVE_LED3)
  disk_light_off_time=timer+DisableDiskLightAfter;
#endif
  fdc_str=BYTE(FDC_STR_BUSY | FDC_STR_MOTOR_ON | WriteProtect);
  floppy_irq_flag=0;
  if (floppy_access_ff) floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
#if defined(SSE_WD1772_393) // type II
  if(!ADAT)
#elif defined(SSE_FDC_ACCURATE_BEHAVIOUR)
  if(ADAT)
    fdc_str|=FDC_STR_T1_SPINUP_COMPLETE; // no real use...
  else
#endif
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,MILLISECONDS_TO_HBLS(1800),0);
  }
        
instant_sector_access_loop:
  int Part=LOWORD(Data);
  int SectorStage=(Part % 71); // 0=seek, 1-64=read/write, 65=end of sector, 66-70=gap
  if (SectorStage==0){
    if (floppy->SeekSector(floppy_current_side(),floppy_head_track[floppyno],fdc_sr,FromFormat)){
      // Error seeking sector, it doesn't exist
      TRACE_LOG("H%d T%d RNF %d\n",floppy_current_side(),floppy_head_track[floppyno],fdc_sr);
      floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
    }
#ifdef ONEGAME
     else{
      if (Command & 0x20){ // Write
        OGWriteSector(floppy_current_side(),floppy_head_track[floppyno],fdc_sr,floppy->BytesPerSector);
      }
    }
#endif
  }else if (SectorStage<=64){
    FILE *f=(FILE*)(FromFormat ? floppy->Format_f:floppy->f);
    int BytesPerStage=16; //SS constant
    int PosInSector=(SectorStage-1)*BytesPerStage;
    BYTE Temp;
    
    if (Command & 0x20){ // Write
      
      if (floppy->ReadOnly){
        floppy_irq_flag=FLOPPY_IRQ_NOW; //interrupt with write-protect flag
        FDCCantWriteDisplayTimer=timer+3000;
      }else{
        floppy->WrittenTo=true;
        if (floppy->IsZip()) FDCCantWriteDisplayTimer=timer+5000; // Writing will be lost!
        //SS byte per byte, we write 16 bytes
        for (int bb=BytesPerStage;bb>0;bb--){
#if defined(SSE_DMA_FIFO_FDC)
          Temp=Dma.GetFifoByte(); //SS from RAM to disk
#else
          Temp=read_from_dma(); //SS from RAM to disk
#endif
#if defined(SSE_BUGFIX_392)
          if (f==NULL || fwrite(&Temp,1,1,f)==0){
#else
          if (fwrite(&Temp,1,1,f)==0){
#endif
            if (floppy_handle_file_error(floppyno,true,fdc_sr,PosInSector,FromFormat)){
              TRACE_LOG("fwrite error\n");
              floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
              break;
            }
          }
#if !(defined(SSE_DMA_FIFO_FDC))
          dma_address++;
#endif
          PosInSector++;
        }
      }
    }else{ // SS Read

#if !(defined(SSE_DMA_FIFO_FDC))
      BYTE *lpDest;
#endif
      for (int bb=BytesPerStage;bb>0;bb--){	// SS: int BytesPerStage=16;

#if defined(SSE_DMA_FIFO_FDC)
#if defined(SSE_BUGFIX_392)
        if (f==NULL || fread(&Temp,1,1,f)==0){
#else
        if (fread(&Temp,1,1,f)==0){
#endif
          if (floppy_handle_file_error(floppyno,0,fdc_sr,PosInSector,FromFormat)){
            TRACE_LOG("fread error\n");
            floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
            break;
          }
        }
        if(dma_sector_count 
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
          && (ADAT||DMA_ADDRESS_IS_VALID_W)
#endif
          ) //game Sabotage
          Dma.AddToFifo(Temp); // disk to RAM
#else
        if (DMA_ADDRESS_IS_VALID_W && dma_sector_count){
          lpDest=lpPEEK(dma_address);
        }else{
          lpDest=&Temp;
        }
        if (fread(lpDest,1,1,f)==0){
          if (floppy_handle_file_error(floppyno,0,fdc_sr,PosInSector,FromFormat)){
            floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
            break;
          }
        }
        DMA_INC_ADDRESS;
#endif
        PosInSector++;
      }
    }
    if (PosInSector>=int(FromFormat ? floppy->FormatLargestSector:floppy->BytesPerSector)){
      Part=64; // Done sector, last part
    }
  }else if (SectorStage==65){
    dbg_log(Str("FDC: Finished reading/writing sector ")+fdc_sr+" of track "+floppy_head_track[floppyno]+" of side "+floppy_current_side());
    floppy_irq_flag=FLOPPY_IRQ_NOW;
    if (Command & BIT_4){ // Multiple sectors
      fdc_sr++;
      floppy_irq_flag=0;
#if defined(SSE_FDC_ACCURATE)
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
      if(true)
#else
      if(ADAT)
#endif
      {
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
        if(!fdc_sr)
          fdc_sr++; // CAPS: 255 wraps to 1 (due to add with carry)
#endif
        TRACE_LOG("SR->%d\n",fdc_sr);
#if defined(SSE_FDC_ACCURATE_TIMING)
        // The controller must find next sector
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
        if(!ADAT)
          agenda_add(agenda_floppy_readwrite_sector,1,MAKELONG(++Part,Command));
        else
#endif
        agenda_add(agenda_floppy_readwrite_sector,Disk[DRIVE].SectorGap(),
          MAKELONG(++Part,Command));
        return;
#endif
      }
#endif
    }
  }

  Part++;

  switch (floppy_irq_flag){
    case FLOPPY_IRQ_NOW:
      dbg_log("FDC: Finished read/write sector");
      fdc_str=BYTE(WriteProtect | FDC_STR_MOTOR_ON); //SS War Heli
      agenda_fdc_finished(0);
      return; // Don't loop
    case FLOPPY_IRQ_ONESEC:
      // sector not found
      fdc_str=BYTE(WriteProtect | FDC_STR_MOTOR_ON | /*FDC_STR_SEEK_ERROR | */FDC_STR_BUSY);
      TRACE_LOG("Sector not found\n");
      agenda_add(agenda_fdc_finished,FDC_HBLS_PER_ROTATION*int((shifter_freq==MONO_HZ) ? 11:5),0);
      return; // Don't loop
  }

  if (floppy_instant_sector_access){  //SS Fast disk
    Data=MAKELONG(Part,Command); // SS Part has been ++, Command is unchanged
    goto instant_sector_access_loop; //SS OMG!
  }else{ //SS Slow disk
#if defined(SSE_FDC_ACCURATE_TIMING)
/*  Correct drift due to hbl system imprecision. 
    With packs of 16 bytes it can accumulate, and at the end of the sector, 
    we're off by a couple of HBL, enough to miss next ID.
    Necrosys STE, Oh No More Froggies
    MPS Golf still OK (MUST slow down)
*/
    ASSERT(Part!=64||SF314[DRIVE].ImageType.Extension==EXT_STT); // 32 -> 65
    WORD bytes= (Part==65)? 19 : 16;
    WORD start=HIWORD(Data);
    if(Part<65)
    {
      WORD current_byte=SF314[DRIVE].BytePosition();
      WORD theory_byte=start+ (Part-1)*16;
      if(current_byte-theory_byte)
      {
        //TRACE_LOG("Drift start %d theory %d byte %d Part %d\n",start,start+(Part-1)*16,SF314[DRIVE].BytePosition(),Part);
        bytes-=2*(current_byte-theory_byte);
      }
    }
    agenda_add(agenda_floppy_readwrite_sector,SF314[DRIVE].BytesToHbls(bytes),
      MAKELONG(Part,start)); 
#else
    // 8000 bytes per revolution * 5 revolutions per second
    int bytes_per_second=8000*5;
    int hbls_per_second=HBLS_PER_SECOND_AVE; // 60hz and 50hz are roughly the same
    if (shifter_freq==MONO_HZ) hbls_per_second=int(HBLS_PER_SECOND_MONO);
    int n_hbls=hbls_per_second/(bytes_per_second/16);
    agenda_add(agenda_floppy_readwrite_sector,n_hbls,MAKELONG(Part,Command));
#endif
  }
}
//---------------------------------------------------------------------------
void agenda_floppy_read_address(int idx)
{
  ASSERT( fdc_cr==0xC0 );
  int floppyno=floppy_current_drive();
  TFloppyImage *floppy=&FloppyDrive[floppyno];
  FDC_IDField IDList[30];
  int nSects=floppy->GetIDFields(floppy_current_side(),floppy_head_track[floppyno],IDList);
  if (idx<nSects){
    dbg_log(Str("FDC: Reading address for sector ")+IDList[idx].SectorNum+" on track "+
                floppy_head_track[floppyno]+", side "+floppy_current_side()+" hbls="+hbl_count);
#if defined(SSE_DMA_FIFO_FDC_READ_ADDRESS)
    BYTE fdc_read_address_buffer[6];
    int fdc_read_address_buffer_len=0;//not real len, we use Dma.Fifo_idx
#endif
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].Track;
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].Side;
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].SectorNum;
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].SectorLen;
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].CRC1;
    fdc_read_address_buffer[fdc_read_address_buffer_len++]=IDList[idx].CRC2;

    TRACE_LOG("Read address T%d S%d s#%d t%d CRC %X %X\n", IDList[idx].Track,
      IDList[idx].Side,IDList[idx].SectorNum,IDList[idx].SectorLen,
      IDList[idx].CRC1,IDList[idx].CRC2);

#if defined(SSE_DMA_FIFO_FDC_READ_ADDRESS)
    for(int i=0;i<6;i++)
      Dma.AddToFifo(fdc_read_address_buffer[i]);
#else
    if (fdc_read_address_buffer_len>=16){ // DMA buffering madness
      for (int n=0;n<16;n++) write_to_dma(fdc_read_address_buffer[n]);
      memmove(fdc_read_address_buffer,fdc_read_address_buffer+16,4);
      fdc_read_address_buffer_len-=16;
    }
#endif

    fdc_str&=BYTE(~FDC_STR_WRITE_PROTECT);
    fdc_str|=FDC_STR_MOTOR_ON;

#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
/*  The Track Address of the ID field is written into the sector register so 
    that a comparison can be made by the user.
    This happens after eventual DMA transfer, so the value should be correct.
*/
#if !defined(SSE_FDC_ACCURATE_BEHAVIOUR_FAST)
    if(ADAT)
#endif
      WD1772.SR=IDList[idx].Track;
#endif
    // TODO, timing?
    agenda_fdc_finished(0);
#if !defined(SSE_OSD_DRIVE_LED3)
    disk_light_off_time=timer+DisableDiskLightAfter;
#endif

#if defined(SSE_FDC_ACCURATE_BEHAVIOUR) //timing...
    if(!ADAT) 
#endif
    {
      agenda_delete(agenda_fdc_motor_flag_off);
      agenda_add(agenda_fdc_motor_flag_off,MILLISECONDS_TO_HBLS(1800),0);
    }
  }
}
//---------------------------------------------------------------------------

/*  SS Read track and write track emulation were good enough to work
    with ProCopy on ST images (amazing!): this mustn't be lost.
    Improvements would relate more to other image formats.
    Also the DMA movements have been left as is for now. In a later version
    we could try to use our DMA Fifo instead.
    This would allow us to see bytes read.
*/

void agenda_floppy_read_track(int part)
{
  ASSERT((fdc_cr&0xF0)==0xE0); //we see $E0, $E4, $E8
#if defined(SSE_VAR_RESIZE) && defined(SSE_WD1772_CRC)
  static short BytesRead;
#define CRC WD1772.CrcLogic.crc
#else
  static int BytesRead;
  static WORD CRC;
#endif
  int floppyno=floppy_current_drive();
  TFloppyImage *floppy=&FloppyDrive[floppyno];
  bool Error=0;
  bool FromFormat=0;

  if (floppy_head_track[floppyno]<=FLOPPY_MAX_TRACK_NUM){
    FromFormat=floppy->TrackIsFormatted[floppy_current_side()][floppy_head_track[floppyno]];
  }
  if (floppy->Empty()) return; // Stop, timeout
#if defined(SSE_FDC_ACCURATE_TIMING)
  int RealPart=HIWORD(part);//starts at 0
  part=LOWORD(part);
#endif

  if (part==0) BytesRead=0;
#if !defined(SSE_OSD_DRIVE_LED3)
  disk_light_off_time=timer+DisableDiskLightAfter;
#endif
  fdc_str|=FDC_STR_BUSY;
  if (floppy_access_ff) floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;

#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
  if(!ADAT)
#endif
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,MILLISECONDS_TO_HBLS(1800),0);
  }

  int TrackBytes=floppy->GetRawTrackData(floppy_current_side(),floppy_head_track[floppyno]);
  if (TrackBytes){
    fseek(floppy->f,BytesRead,SEEK_CUR);
    int ReinsertAttempts=0;
    BYTE Temp,*lpDest;
    for (int n=0;n<16;n++){
      if (BytesRead>=TrackBytes) break;

      if (DMA_ADDRESS_IS_VALID_W && dma_sector_count){
        lpDest=lpPEEK(dma_address);
      }else{
        lpDest=&Temp;
      }
      if (fread(lpDest,1,1,floppy->f)==0){
        if (ReinsertAttempts++ > 2){
          Error=true;
          break;
        }
        if (floppy->ReinsertDisk()){
          TrackBytes=floppy->GetRawTrackData(floppy_current_side(),floppy_head_track[floppyno]);
          fseek(floppy->f,BytesRead,SEEK_CUR);
          n--;
        }
      }else{
        DMA_INC_ADDRESS;
        BytesRead++;
      }
    }
  }else{
    int DDBytes=6272-(floppy_head_track[floppyno]/25)*16; // inner tracks (higher numbers) are a bit shorter
    FDC_IDField IDList[30];
    int nSects=floppy->GetIDFields(floppy_current_side(),floppy_head_track[floppyno],IDList);
    if (nSects==0){
      // Unformatted track, read in random values
      TrackBytes=DDBytes;
      for (int bb=0;bb<16;bb++){
        write_to_dma(rand());
        BytesRead++;
      }
    }else{
      // Find out if it is a high density track
      TrackBytes=0;
      for (int n=0;n<nSects;n++) TrackBytes+=22+12+3+1+6+22+12+3+1 + (128 << IDList[n].SectorLen) + 26;
      if (TrackBytes>DDBytes){
        TrackBytes=DDBytes*2;
      }else{
        TrackBytes=DDBytes;
      }
#if defined(SSE_FDC_ACCURATE_TIMING)
      if(ADAT) // should be 6256 up to 11 secs, beyond, it's not ADAT
        TrackBytes=Disk[floppyno].TrackBytes; 
#endif
      if (part/154<nSects){
#if defined(SSE_DMA_TRACK_TRANSFER)
        Dma.Datachunk++;
#endif
        int IDListIdx=part/154;
        BYTE SectorNum=IDList[IDListIdx].SectorNum;
        int SectorBytes=(128 << IDList[IDListIdx].SectorLen);
        BYTE pre_sect[200];
        int i=0;
#if defined(SSE_FDC_ACCURATE_TIMING)
        for (int n=0;n<(ADAT?nSects<Disk[floppyno].PostIndexGap():22);n++) 
          pre_sect[i++]=0x4e;  // Gap 1 & 3 (22 bytes)
/*
Gap 2 Pre ID                    12+3        12+3         3+3     00+A1
*/
        for (int n=0;n< (nSects<11?12:3);n++) pre_sect[i++]=0x00;

#else
        for (int n=0;n<22;n++) pre_sect[i++]=0x4e;  // Gap 1 & 3 (22 bytes)
        for (int n=0;n<12;n++) pre_sect[i++]=0x00;  // Gap 3 (12)
#endif  
        for (int n=0;n<3;n++) pre_sect[i++]=0xa1;   // Marker
        pre_sect[i++]=0xfe;                         // Start of address mark
        pre_sect[i++]=IDList[IDListIdx].Track;
        pre_sect[i++]=IDList[IDListIdx].Side;
        pre_sect[i++]=IDList[IDListIdx].SectorNum;
        pre_sect[i++]=IDList[IDListIdx].SectorLen;
        pre_sect[i++]=IDList[IDListIdx].CRC1;
        pre_sect[i++]=IDList[IDListIdx].CRC2;
        for (int n=0;n<22;n++) pre_sect[i++]=0x4e; // Gap 2
        for (int n=0;n<12;n++) pre_sect[i++]=0x00; // Gap 2
        for (int n=0;n<3;n++) pre_sect[i++]=0xa1;  // Marker
/*
Data Address Mark                  1           1           1      FB
*/
        pre_sect[i++]=0xfb;                        // Start of data
        int num_bytes_to_write=16;
        int byte_idx=(part % 154)*16;

        // Write the gaps/address before the sector
        if (byte_idx<i){
          while (num_bytes_to_write>0){
            write_to_dma(pre_sect[byte_idx++]);
            num_bytes_to_write--;
            BytesRead++;
            if (byte_idx>=i) break;
          }
        }
        byte_idx-=i;
        // Write the sector
/*
Data                             512         512         512
*/
        if (num_bytes_to_write>0 && byte_idx>=0 && byte_idx<SectorBytes){
          if (byte_idx==0){
            CRC=0xffff;
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xfb);
          }
          if (floppy->SeekSector(floppy_current_side(),floppy_head_track[floppyno],
                                    SectorNum,FromFormat)){
            // Can't seek to sector!
            TRACE_LOG("Argh! Can't seek to sector!\n");
            while (num_bytes_to_write>0){
              write_to_dma(0x00);
              fdc_add_to_crc(CRC,0x00);
              num_bytes_to_write--;
              BytesRead++;
              byte_idx++;
              if (byte_idx>=SectorBytes) break;
            }
          }else{
            FILE *f=(FILE*)(FromFormat ? floppy->Format_f:floppy->f);
            fseek(f,byte_idx,SEEK_CUR);
            BYTE Temp,*pDest;

            for (;num_bytes_to_write>0;num_bytes_to_write--){
              if (DMA_ADDRESS_IS_VALID_W && dma_sector_count){
                pDest=lpPEEK(dma_address);
              }else{
                pDest=&Temp;
              }
              if (fread(pDest,1,1,f)==0){
                if (floppy_handle_file_error(floppyno,0,SectorNum,byte_idx,FromFormat)){
                  fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;
                  Error=true;
                  num_bytes_to_write=0;
                  break;
                }
              }
              fdc_add_to_crc(CRC,*pDest);
              DMA_INC_ADDRESS;
              BytesRead++;
              byte_idx++;
              if (byte_idx>=SectorBytes) break;
            }
          }
        }
        byte_idx-=SectorBytes;

        // Write CRC
/*
CRC                                2           2           2
*/
        if (num_bytes_to_write>0 && byte_idx>=0 && byte_idx<2){
          if (byte_idx==0){
            write_to_dma(HIBYTE(CRC));          // End of Data Field (CRC)
            byte_idx++;
            BytesRead++;
            num_bytes_to_write--;
          }
          if (byte_idx==1 && num_bytes_to_write>0){
            write_to_dma(LOBYTE(CRC));          // End of Data Field (CRC)
            byte_idx++;
            BytesRead++;
            num_bytes_to_write--;
          }
        }
        byte_idx-=2;

        // Write Gap 4
#if defined(SSE_FDC_ACCURATE_TIMING)
/*
Gap 4 Post Data                   40          40           1      4E
*/
        BYTE gap4bytes=(nSects>=11?1:24);
        if (num_bytes_to_write>0 && byte_idx>=0 && byte_idx<gap4bytes){
          while (num_bytes_to_write>0){
            write_to_dma(0x4e);
            byte_idx++;
            num_bytes_to_write--;
            BytesRead++;
            if (byte_idx>=gap4bytes){
              // Move to next sector (-1 because we ++ below)
              part=(IDListIdx+1)*154-1;
              break;
            }
          }
        }
#else
        if (num_bytes_to_write>0 && byte_idx>=0 && byte_idx<24){
          while (num_bytes_to_write>0){
            write_to_dma(0x4e);
            byte_idx++;
            num_bytes_to_write--;
            BytesRead++;
            if (byte_idx>=24){
              // Move to next sector (-1 because we ++ below)
              part=(IDListIdx+1)*154-1;
              break;
            }
          }
        }
#endif
      }else{
        // End of track, read in 0x4e
#if defined(SSE_FDC_ACCURATE_TIMING)
        BYTE gap5bytes=(nSects>=11?20:16); //ProCopy 1.5 Analyze
        // isn't it a bug anyway? More gap with 11 than 9-10 ???
        write_to_dma(0x4e,gap5bytes);
        BytesRead+=gap5bytes;
#else
        write_to_dma(0x4e,16);
        BytesRead+=16;
#endif
      }
    }
  }
  part++;

  if (BytesRead>=TrackBytes){//finished reading in track
    fdc_str=FDC_STR_MOTOR_ON;  //all fine!
    agenda_fdc_finished(0);
    dbg_log(Str("FDC: Read track finished, t=")+hbl_count);
  }else if (Error==0){   //read more of the track
#if defined(SSE_FDC_ACCURATE_TIMING)
    int bytes_per_second=Disk[DRIVE].TrackBytes*5;
    int hbls_per_second=HBL_PER_SECOND;
#else
    // 8000 bytes per revolution * 5 revolutions per second
    int bytes_per_second=8000*5;
    int hbls_per_second=HBLS_PER_SECOND_AVE; // 60hz and 50hz are roughly the same
    if (shifter_freq==MONO_HZ) hbls_per_second=int(HBLS_PER_SECOND_MONO);
#endif
    int n_hbls=hbls_per_second/(bytes_per_second/16);
#if defined(SSE_FDC_ACCURATE_TIMING) 
/*  Correct HBL drift for Read Track.
    Timing is important for ProCopy Analyze.
*/
    WORD current_byte=SF314[DRIVE].BytePosition();
    WORD theory_byte=RealPart*16;
    if(current_byte>theory_byte) //only one way
    {
//      TRACE_LOG("TRACK READ Drift theory %d byte %d #hbl %d\n",RealPart*16,SF314[DRIVE].BytePosition(),n_hbls);
      n_hbls--;
    }
    RealPart++;
    agenda_add(agenda_floppy_read_track,n_hbls,MAKELONG(part,RealPart)); 
#else
    agenda_add(agenda_floppy_read_track,n_hbls,part); 
#endif
  }
}


/*
DISK FORMATTING:
----------------

The 177x formats disks according to the IBM 3740 or System/34
standard.  See the Write Track command for the CPU formatting method.
The recommended physical format for 256-byte sectors is as follows.

Number of Bytes     Value of Byte      Comments
---------------     -------------      --------
60                  $4e                Gap 1 and Gap 3.  Start and end of index
                                       pulse.
12                  $00                Gap 3.  Start of bytes repeated for each
                                       sector.
3                   $a1                Gap 3.  Start of ID field.  See section
                                       on Write Track command.
1                   $fe                ID address mark
1                   track #            $00 through $4c (0 through 76)
1                   side #             0 or 1
1                   sector #           $01 through $10 (1 through 16)
1                   length code        See section on Read Address command.
2                   CRC                End of ID field.  See section on Write
                                       Track command.
22                  $4e                Gap 2.
12                  $00                Gap 2.  During Write Sector commands the
                                       drive starts writing at the start
                                       of this.
3                   $a1                Gap 2.  Start of data field.  See
                                       section on Write Track command.
1                   $fb                data address mark
256                 data               Values $f5, $f6, and $f7 invalid.  See
                                       section on Write Track command.  IBM
                                       uses $e5.
2                   CRC                End of data field.  See section on Write
                                       Track command.
24                  $4e                Gap 4.  End of bytes repeated for each
                                       sector.  During Write Sector
                                       commands the drive stops writing shortly
                                       after the beginning of this.
668                 $4e                Continue writing until the 177x
                                       generates an interrupt.  The listed byte
                                       count is approximate.

Variations in the recommended formats are possible if the following
requirements are met:
(1)  Sector size must be 128, 256, 512, or 1024 bytes.
(2)  All address mark indicators ($a1) must be 3 bytes long.
(3)  The $4e section of Gap 2 must be 22 bytes long.  The $00 section of Gap 2
     must be 12 bytes long.
(4)  The $4e section of Gap 3 must be at least 24 bytes long.  The $00 section
     of Gap 3 must be at least 8 bytes long.
(5)  Gaps 1 and 4 must be at least 2 bytes long.  These gaps should be longer
     to allow for PLL lock time, motor speed variations, and write splice time.

The 177x does not require an Index Address Mark.
*/

void agenda_floppy_write_track(int part)
{
  ASSERT((fdc_cr&0xF0)==0xF0);
  static int SectorLen,nSector=-1;
  int floppyno=floppy_current_drive();
  TFloppyImage *floppy=&FloppyDrive[floppyno];
  BYTE Data;
  int TrackBytes=6448; // Double density format only
#if defined(SSE_FDC_ACCURATE_TIMING)
  if(ADAT)
     TrackBytes=Disk[floppyno].TrackBytes; 
#endif
  if (floppy->ReadOnly || floppy->STT_File){ //SS STT is no solution for Format
    fdc_str=FDC_STR_MOTOR_ON | FDC_STR_WRITE_PROTECT;
    agenda_fdc_finished(0);
    return;
  }
  if (floppy->Empty()){
    fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;
    return;
  }

  floppy->WrittenTo=true;

  bool Error=0;
  fdc_str|=FDC_STR_BUSY;
  if (floppy_access_ff) floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
#if defined(SSE_FDC_ACCURATE_BEHAVIOUR)
  if(!ADAT)
#endif
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,MILLISECONDS_TO_HBLS(1800),0);
  }

#if !defined(SSE_OSD_DRIVE_LED3)
  disk_light_off_time=timer+DisableDiskLightAfter;
#endif
  if (part==0){ // Find data/address marks
    // Find address marks and read SectorLen and nSector from the address
    // Must have [[0xa1] 0xa1] 0xa1 0xfe or [[0xc2] 0xc2] 0xc2 0xfe

    // Find data marks and increase part
    // Must have [[0xa1] 0xa1] 0xa1 0xfb or [[0xc2] 0xc2] 0xc2 0xfb
    for (int n=0;n<16;n++){
      Data=read_from_dma();
      dma_address++;
      floppy_write_track_bytes_done++;
      if (Data==0xa1 || Data==0xf5 || Data==0xc2 || Data==0xf6){ // Start of gap 3
        int Timeout=10;
        do{
          Data=read_from_dma();
          dma_address++;floppy_write_track_bytes_done++;
        }while ((Data==0xa1 || Data==0xf5 || Data==0xc2 || Data==0xf6) && (--Timeout)>0);
        if (Data==0xfe){ // Found address mark
          if (dma_address+4<himem){
            nSector=PEEK(dma_address+2);
            switch (PEEK(dma_address+3)){
              case 0:  SectorLen=128;break;
              case 1:  SectorLen=256;break;
              case 2:  SectorLen=512;break;
              case 3:  Error=true /*SectorLen=1024*/ ;
                TRACE_LOG("Format 1024!\n");
                break; //SS why?
              default: Error=true;
            }
            if (Error){
              dbg_log(Str("FDC: Format data with invalid sector length (")+PEEK(dma_address+3)+"). Skipping this ID field.");
              Error=0;
            }
          }
        }else if (Data==0xfb){
          part++; // Read next SectorLen bytes of data
          break;
        }
      }
    }
  }else{
    bool IgnoreSector=true;
    if (nSector>=0){
      IgnoreSector=floppy->SeekSector(floppy_current_side(),floppy_head_track[floppyno],nSector,true);
    }
    if (IgnoreSector){
      LOG_ONLY( if (nSector<0) dbg_log("FDC: Format sector data with no address, it will be lost in the ether"); )
      LOG_ONLY( if (nSector>=0) dbg_log("FDC: Format can't write sector, sector number too big for this type of image"); )

      dma_address+=SectorLen;
      floppy_write_track_bytes_done+=SectorLen;
      part=0;
      nSector=-1;
    }else{
      fseek(floppy->Format_f,(part-1)*16,SEEK_CUR);

      floppy->FormatMostSectors=max(nSector,floppy->FormatMostSectors);
      floppy->FormatLargestSector=max(SectorLen,floppy->FormatLargestSector);
      floppy->TrackIsFormatted[floppy_current_side()][floppy_head_track[floppyno]]=true;

      for (int bb=0;bb<16;bb++){
        Data=read_from_dma();dma_address++;
/*
        if (Data==0xf5){
          Data=0xa1;
        }else if (Data==0xf6){
          Data=0xc2;
        }else if (Data==0xf7){
          WriteCRC=true;
          Data=0;
        }
*/
        if (fwrite(&Data,1,1,floppy->Format_f)==0){
          Error=true;
          if (floppy->ReopenFormatFile()){
            floppy->SeekSector(floppy_current_side(),floppy_head_track[floppyno],nSector,true);
            fseek(floppy->Format_f,(part-1)*16 + bb,SEEK_CUR);
            Error=(fwrite(&Data,1,1,floppy->Format_f)==0);
          }
        }
        if (Error) break;

        floppy_write_track_bytes_done++;
      }
      part++;
      if ((part-1)*16>=SectorLen){
        nSector=-1;
        part=0;
      }
    }
  }
  if (floppy_write_track_bytes_done>TrackBytes){
    dbg_log(Str("FDC: Format finished, wrote ")+floppy_write_track_bytes_done+" bytes");
    fdc_str=FDC_STR_MOTOR_ON;  //all fine!
    agenda_fdc_finished(0);
    fflush(floppy->Format_f);
  }else if (Error){
    dbg_log("FDC: Format aborted, can't write to format file");
    fdc_str=FDC_STR_MOTOR_ON | FDC_STR_SEEK_ERROR | FDC_STR_BUSY;
  }else{ //write more of the track
#if defined(SSE_FDC_ACCURATE_TIMING)
    int bytes_per_second=(ADAT)? Disk[DRIVE].TrackBytes*5 : 8000*5;
    int hbls_per_second=(ADAT)? HBL_PER_SECOND : 
      ((shifter_freq==MONO_HZ)?int(HBLS_PER_SECOND_MONO) : HBLS_PER_SECOND_AVE);
#else
    // 8000 bytes per revolution * 5 revolutions per second
    int bytes_per_second=8000*5;
    int hbls_per_second=HBLS_PER_SECOND_AVE; // 60hz and 50hz are roughly the same
    if (shifter_freq==MONO_HZ) hbls_per_second=int(HBLS_PER_SECOND_MONO);
#endif
    int n_hbls=hbls_per_second/(bytes_per_second/16);
    agenda_add(agenda_floppy_write_track,n_hbls,part);
  }
}
//---------------------------------------------------------------------------
void fdc_add_to_crc(WORD &crc,BYTE data)
{
  // The CRC polynomial is x^16+x^12+x^5+1.
  for (int i=0;i<8;i++){
    crc = WORD((crc << 1) ^ ((((crc >> 8) ^ (data << i)) & 0x0080) ? 0x1021 : 0));
  }
}
//---------------------------------------------------------------------------

#if USE_PASTI

void pasti_handle_return(struct pastiIOINFO *pPIOI)
{
//  log_to(LOGSECTION_PASTI,Str("PASTI: Handling return, update cycles=")+pPIOI->updateCycles+" irq="+pPIOI->intrqState+" Xfer="+pPIOI->haveXfer);

  pasti_update_time=ABSOLUTE_CPU_TIME+pPIOI->updateCycles; //SS smart...
  
  bool old_irq=(mfp_reg[MFPR_GPIP] & BIT_5)==0; // 0=irq on

#if defined(SSE_COMPILER_WARNING)
  if (old_irq!=(bool)pPIOI->intrqState){
#else
  if (old_irq!=pPIOI->intrqState){
#endif

#if defined(SSE_DISK_GHOST)
    if(!WD1772.Lines.CommandWasIntercepted)
#endif
      mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,!pPIOI->intrqState);
    if (pPIOI->intrqState){
      // FDC is busy, activate light
#if !defined(SSE_OSD_DRIVE_LED3)
      disk_light_off_time=timer+DisableDiskLightAfter;
#endif

#if defined(SSE_DRIVE_SOUND) && !defined(SSE_BUGFIX_392)
      if(SSEOption.DriveSound)
        SF314[DRIVE].Sound_CheckIrq();
#endif

    }

  }

  //SS pasti manages DMA itself, then calls Steem to transfer what it gathered

  if (pPIOI->haveXfer){
    dma_address=pPIOI->xferInfo.xferSTaddr; //SS not the regs

    if (pPIOI->xferInfo.memToDisk){
      //log_to(LOGSECTION_PASTI,Str("PASTI: DMA transfer ")+pPIOI->xferInfo.xferLen+" bytes from address=$"+HEXSl(dma_address,6)+" to pasti buffer");
      for (DWORD i=0;i<pPIOI->xferInfo.xferLen;i++){
        if (DMA_ADDRESS_IS_VALID_R) 
        {
#if defined(SSE_DMA_FIFO_PASTI)
          LPBYTE(pPIOI->xferInfo.xferBuf)[i]=Dma.GetFifoByte();
#else
          LPBYTE(pPIOI->xferInfo.xferBuf)[i]=PEEK(dma_address);
#endif
        }
#if !(defined(SSE_DMA_FIFO_PASTI))
        dma_address++;
#endif
      }
    }else{
      //log_to(LOGSECTION_PASTI,Str("PASTI: DMA transfer ")+pPIOI->xferInfo.xferLen+" bytes from pasti buffer to address=$"+HEXSl(dma_address,6));
      for (DWORD i=0;i<pPIOI->xferInfo.xferLen;i++){ 
        if (DMA_ADDRESS_IS_VALID_W){
#if !defined(SSE_BOILER_MONITOR_VALUE2)
          DEBUG_CHECK_WRITE_B(dma_address);
#endif
#if defined(SSE_DMA_FIFO_PASTI)
          Dma.AddToFifo(LPBYTE(pPIOI->xferInfo.xferBuf)[i]);
#else
          PEEK(dma_address)=LPBYTE(pPIOI->xferInfo.xferBuf)[i];
#endif
#if defined(SSE_BOILER_MONITOR_VALUE2)
          DEBUG_CHECK_WRITE_B(dma_address);
#endif
        }
#if !(defined(SSE_DMA_FIFO_PASTI))
        dma_address++;
#endif
      }
    }
  }
    
#if defined(SSE_DMA) && defined(SSE_DEBUG)
  if(TRACE_ENABLED(LOGSECTION_FDC)&&!old_irq&&old_irq!=(bool)pPIOI->intrqState) 
    Dma.UpdateRegs(true);
#endif

  if (pPIOI->brkHit){
    if (runstate==RUNSTATE_RUNNING){
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Pasti breakpoint");
    }
    DEBUG_ONLY( if (debug_in_trace) SET_WHY_STOP("Pasti breakpoint"); )
  }
  ioaccess|=IOACCESS_FLAG_FOR_CHECK_INTRS;


#if defined(SSE_DRIVE_SOUND) && defined(SSE_BUGFIX_392)
  Dma.UpdateRegs();
  if(OPTION_DRIVE_SOUND && pPIOI->intrqState&&!old_irq)
    SF314[DRIVE].Sound_CheckIrq();
#endif

}

#if defined(SSE_VS2008_WARNING_390) && defined(SSE_OSD_DRIVE_LED3)
void pasti_motor_proc(BOOL)
#else
void pasti_motor_proc(BOOL on)
#endif
{
#if !defined(SSE_OSD_DRIVE_LED3)
  disk_light_off_time=timer;
  if (on) disk_light_off_time+=50*1000;
#endif
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_PASTI

void pasti_log_proc(const char * LOG_ONLY(text))
{
#if defined(SSE_DEBUG_LOG_OPTIONS) \
  && defined(ENABLE_LOGFILE) && !defined(SSE_BOILER_390_LOG2)
  TRACE_LOG("Pasti: %s\n",(char*)text);
  // it was the only active logging for this section 
  dbg_log(text);
//#else
#elif defined(DEBUG_BUILD)
  TRACE_LOG("Pasti: %s\n",(char*)text);
  log_to(LOGSECTION_PASTI,Str("PASTI: ")+text);
#endif
}

void pasti_warn_proc(const char *text)
{
  Alert((char*)text,"Pasti Warning",0);
}

#if defined(SSE_DISK_GHOST_SECTOR_STX1)
/*  Little function to update the value of some register inside
    pasti, hopefully without triggering anything.
*/
void pasti_update_reg(unsigned addr,unsigned data) {
  struct pastiIOINFO pioi;
  pioi.stPC=pc; 
  pioi.cycles=ABSOLUTE_CPU_TIME;    
  pioi.addr=addr;
  pioi.data=data;
  pasti->Io(PASTI_IOWRITE,&pioi);
}
#endif

#endif//#if USE_PASTI
//---------------------------------------------------------------------------
#undef LOGSECTION
