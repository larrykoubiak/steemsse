/*---------------------------------------------------------------------------
FILE: ior.cpp
MODULE: emu
DESCRIPTION: I/O address reads. This file contains crucial core functions
that deal with reads from ST I/O addresses ($ff8000 onwards).
---------------------------------------------------------------------------*/

#if defined(STEVEN_SEAGAL) && defined(SS_STRUCTURE)
#pragma message("Included for compilation: ior.cpp")
#endif

#define LOGSECTION LOGSECTION_IO
#if !defined(STEVEN_SEAGAL) || !defined(SS_VID_SDP_READ) || defined(SS_DEBUG)
MEM_ADDRESS get_shifter_draw_pointer(int cycles_since_hbl)
{ // SS: basis for TShifter::ReadSDP in SSEVideo.h
  if (bad_drawing){
    // Fake SDP
    if (scan_y<0){
      return xbios2;
    }else if (scan_y<shifter_y){
      int line_len=(160/res_vertical_scale);
      return xbios2 + scan_y*line_len + min(cycles_since_hbl/2,line_len) & ~1;
    }else{
      return xbios2+32000;
    }
  }
  draw_check_border_removal();

  if (scan_y>=shifter_first_draw_line && scan_y<shifter_last_draw_line){
    int bytes_ahead=8;
    if (shifter_hscroll_extra_fetch) bytes_ahead=16;
    int starts_counting=CYCLES_FROM_HBL_TO_LEFT_BORDER_OPEN/2 - bytes_ahead;
    int bytes_to_count=160; //160 bytes later

    if (left_border==0){ starts_counting-=26;bytes_to_count+=28; }

    if (right_border==0){
      bytes_to_count+=50; //44
    }else if (shifter_skip_raster_for_hscroll){
      bytes_to_count+=8;
    }
    if (overscan_add_extra<-60) bytes_to_count-=106; //big right border

    int c=cycles_since_hbl/2-starts_counting;
    MEM_ADDRESS sdp=shifter_draw_pointer_at_start_of_line;
    if (c>=bytes_to_count){
      sdp+=bytes_to_count+(shifter_fetch_extra_words*2);
    }else if (c>=0){
      c&=-2;
      sdp+=c;
    }
    return sdp;
  }else{
    return shifter_draw_pointer;
  }
}
#endif
//---------------------------------------------------------------------------
BYTE ASMCALL io_read_b(MEM_ADDRESS addr)
{
/*
  Allowed addresses

  000000 - 3FFFFF   RAM

  D00000 - D7FFFF
  E00000 - E3FFFF   TOS
  E40000 - EBFFFF

  FA0000 - FBFFFF   Cart

  FE0000 - FE1FFF

  FF8000 - FF800F   MMU
  FF8200 - FF820F   SHIFTER
  FF8240 - FF827F   pallette, res
  FF8608 - FF860F   FDC
  FF8800 - FF88FF   sound chip
  FF8900 - FF893F   DMA sound, microwire
  FF8A00 - FF8A3F   blitter
  FF9000 - FF9001   
  FF9201, FF9202, FF9203           paddles
  FF9211, FF9213, FF9215, FF9217   paddles
  FFFA01, odd addresses up to FFFA3F   MFP
  FFFC00 - FFFDFF   ACIA, realtime clock

  Word differences:

  FF8604 - FF860F   FDC
  FF9200, FF9202                   paddles
  FF9210, FF9212, FF9214, FF9216   paddles
  FF9220, FF9222                   paddles
  FFFA00 - FFFA3F   MFP
*/

  DEBUG_CHECK_READ_IO_B(addr);
#ifdef ONEGAME
  if (addr>=OG_TEXT_ADDRESS && addr<OG_TEXT_ADDRESS+OG_TEXT_LEN){
    return BYTE(OG_TextMem[addr-OG_TEXT_ADDRESS]);
  }
#endif
  switch (addr & 0xffff00){  //fffe00
    case 0xfffc00:      //----------------------------------- ACIAs
    {
      // Only cause bus jam once per word
      DEBUG_ONLY( if (mode==STEM_MODE_CPU) )
      {
        if (io_word_access==0 || (addr & 1)==0){

#if defined(STEVEN_SEAGAL) && defined(SS_ACIA_BUS_JAM_NO_WOBBLE)
          const int rel_cycle=0; // hoping it will be trashed by compiler
#else // Steem 3.2, 3.3

//          if (passed VBL or HBL point){ //SS: those // are not mine
//            BUS_JAM_TIME(4);
//          }else{
          // Jorge Cwik:
          // Access to the ACIA is synchronized to the E signal. Which is a clock with
          // one tenth the frequency of the main CPU clock (800 Khz). So the timing
          // should depend on the phase relationship between both clocks.

          int rel_cycle=ABSOLUTE_CPU_TIME-shifter_cycle_base;
#if defined(STEVEN_SEAGAL) && defined(SS_MFP_RATIO)
          rel_cycle=CpuNormalHz-rel_cycle;
#else
          rel_cycle=8000000-rel_cycle;
#endif
          rel_cycle%=10;
#endif

#if defined(STEVEN_SEAGAL) && defined(SS_VID_SHIFTER_EVENTS)
          VideoEvents.Add(scan_y,LINECYCLES,'j',rel_cycle+6);
#endif
          BUS_JAM_TIME(rel_cycle+6); // just 6 - fixes jitter in Spectrum 512
        }
      }
      switch (addr){
/******************** Keyboard ACIA ************************/

#if defined(STEVEN_SEAGAL) && defined(SS_IKBD)

      case 0xfffc00:  //status
      {
#if defined(SS_DEBUG) && defined(SS_ACIA_IRQ_DELAY)
        int elapsed_cycles=ABSOLUTE_CPU_TIME-ikbd.timer_when_keyboard_info;
#endif
        // Build the byte x to be returned based on our ACIA var
        // Note ACIA_IKBD = acia[0].
        BYTE x=0;
        // bit 0
        if(ACIA_IKBD.overrun)
          x|=BIT_0;
        if(ACIA_IKBD.rx_not_read)
          x|=BIT_0; 
        // bit 1: can we send a byte to the 6301 now?
        if(ACIA_IKBD.tx_flag==0 
#if defined(SS_IKBD_6301) 
          &&(!hd6301_receiving_from_MC6850 ||!HD6301EMU_ON)  
#endif
          )
          x|=BIT_1; //empty bit
        // bit 7 (high bit)
        if(ACIA_IKBD.irq) 
        {
          ASSERT( ACIA_IKBD.rx_irq_enabled );
          x|=BIT_7; //irq bit
#if defined(SS_IKBD_TRACE_IO) && defined(SS_ACIA_IRQ_DELAY)
          TRACE("PC %X $FC00 check irq at %d (%d)\n",pc-2,elapsed_cycles,ABSOLUTE_CPU_TIME);
#endif
        }
        // bit 5
        if (ACIA_IKBD.overrun==ACIA_OVERRUN_YES) 
          x|=BIT_5; //overrun

#if defined(SS_ACIA_TRACE_READ_SR)
#if defined(SS_IKBD_TRACE_IO) && defined(SS_ACIA_IRQ_DELAY)
        if(ACIA_IKBD.rx_stage)
          TRACE("PC %X, read 0xfffc00: %x after %d cycles, ACIA rx stage %d\n",pc-2,x,elapsed_cycles,ACIA_IKBD.rx_stage); 
        else if(keyboard_buffer_length)
          TRACE("PC %X, read 0xfffc00: %x\n",pc-2,x); 
#endif
#if defined(SS_IKBD_TRACE_6301)
        if(x&BIT_1) printf("read 0xfffc00 %x ACT %d PX %X\n",x,ABSOLUTE_CPU_TIME,pc);
#endif
#endif
          return x;
        }//scope

#else // Steem 3.2
      case 0xfffc00:  //status
      {
        BYTE x=0;
        if (ACIA_IKBD.rx_not_read || ACIA_IKBD.overrun==ACIA_OVERRUN_YES) x|=BIT_0; //full bit
        if (ACIA_IKBD.tx_flag==0) x|=BIT_1; //empty bit
//        if (acia[ACIA_IKBD].rx_not_read && acia[ACIA_IKBD].rx_irq_enabled) x|=BIT_7; //irq bit
        if (ACIA_IKBD.irq) x|=BIT_7; //irq bit
        if (ACIA_IKBD.overrun==ACIA_OVERRUN_YES) x|=BIT_5; //overrun
        return x;
      }
#endif

      case 0xfffc02:  //data
      {
        DEBUG_ONLY( if (mode!=STEM_MODE_CPU) return ACIA_IKBD.data; )
        ACIA_IKBD.rx_not_read=0; // SS: reading the data register changes the status register
        LOG_ONLY( bool old_irq=ACIA_IKBD.irq; )
        if (ACIA_IKBD.overrun==ACIA_OVERRUN_COMING){
          ACIA_IKBD.overrun=ACIA_OVERRUN_YES;
          if (ACIA_IKBD.rx_irq_enabled) ACIA_IKBD.irq=true;
          LOG_ONLY( log_to_section(LOGSECTION_IKBD,EasyStr("IKBD: ")+HEXSl(old_pc,6)+
                              " - OVERRUN! Read data ($"+HEXSl(ACIA_IKBD.data,2)+
                              "), changing ACIA IRQ bit from "+old_irq+" to "+ACIA_IKBD.irq); )
        }else{
          ACIA_IKBD.overrun=ACIA_OVERRUN_NO;
          // IRQ should be off for receive, but could be set for tx empty interrupt
          ACIA_IKBD.irq=(ACIA_IKBD.tx_irq_enabled && ACIA_IKBD.tx_flag==0);
          LOG_ONLY( if (ACIA_IKBD.irq!=old_irq) log_to_section(LOGSECTION_IKBD,Str("IKBD: ")+
            HEXSl(old_pc,6)+" - Read data ($"+HEXSl(ACIA_IKBD.data,2)+
            "), changing ACIA IRQ bit from "+old_irq+" to "+ACIA_IKBD.irq); )
          }
          mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(ACIA_IKBD.irq || ACIA_MIDI.irq));
#if defined(SS_ACIA_TRACE_IO)
          TRACE("PC %X Read 0xfffc02: %x (%d) at ACT %d\n",pc,ACIA_IKBD.data,hd6301_transmitting_to_MC6850,ABSOLUTE_CPU_TIME);
#endif
          return ACIA_IKBD.data;
      }

  /******************** MIDI ACIA ************************/

      case 0xfffc04:  // status
      {
        BYTE x=0;
        if (ACIA_MIDI.rx_not_read || ACIA_MIDI.overrun==ACIA_OVERRUN_YES) x|=BIT_0; //full bit
        if (ACIA_MIDI.tx_flag==0) x|=BIT_1; //empty bit
        if (ACIA_MIDI.irq) x|=BIT_7; //irq bit
        if (ACIA_MIDI.overrun==ACIA_OVERRUN_YES) x|=BIT_5; //overrun
        return x;
      }
      case 0xfffc06:  // data
        DEBUG_ONLY(if (mode!=STEM_MODE_CPU) return ACIA_MIDI.data);
        ACIA_MIDI.rx_not_read=0;
        if (ACIA_MIDI.overrun==ACIA_OVERRUN_COMING){
          ACIA_MIDI.overrun=ACIA_OVERRUN_YES;
          if (ACIA_MIDI.rx_irq_enabled) ACIA_MIDI.irq=true;
        }else{
          ACIA_MIDI.overrun=ACIA_OVERRUN_NO;
          // IRQ should be off for receive, but could be set for tx empty interrupt
          ACIA_MIDI.irq=(ACIA_MIDI.tx_irq_enabled && ACIA_MIDI.tx_flag==0);
        }
        mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(ACIA_IKBD.irq || ACIA_MIDI.irq));
        log_to(LOGSECTION_MIDI,Str("MIDI: ")+HEXSl(old_pc,6)+" - Read $"+
                HEXSl(ACIA_MIDI.data,6)+" from MIDI ACIA data register");
        return ACIA_MIDI.data;
      }

      break;
    }case 0xfffd00:{      //----------------------------------- ?

      return 0xff;
    }case 0xfffa00:{      //----------------------------------- MFP
      if (addr<0xfffa40){
        // Only cause bus jam once per word (should this be after the read?)
        DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) if (io_word_access==0 || (addr & 1)==1) BUS_JAM_TIME(4);

        BYTE x=0xff;
        if (addr & 1){
          if (addr==0xfffa01){
            // read GPIP
            // BIT
            //  0   Centronics busy
            //  1   RS-232 data carrier detect - input
            //  2   RS-232 clear to send - input
            //  3   Reserved
            //  4   Keyboard and MIDI
            //  5   FDC/HDC
            //  6   RS-232 ring indicator
            //  7   Monochrome monitor detect
            x=BYTE(mfp_reg[MFPR_GPIP] & ~mfp_reg[MFPR_DDR]);
            x|=BYTE(mfp_gpip_input_buffer & mfp_reg[MFPR_DDR]);
          }else if (addr<0xfffa30){
            int n=(addr-0xfffa01) >> 1;
            if (n>=MFPR_TADR && n<=MFPR_TDDR){ //timer data registers
              mfp_calc_timer_counter(n-MFPR_TADR);
              x=BYTE(mfp_timer_counter[n-MFPR_TADR]/64);
              if (n==MFPR_TBDR){
                if (mfp_get_timer_control_register(1)==8){
                  // Timer B is in event count mode, check if it has counted down since the start of
                  // this instruction. Due to MFP delays this very, very rarely gets changed under 4
                  // cycles from the point of the signal.
                  if ((ABSOLUTE_CPU_TIME-time_of_next_timer_b) > 4){
                    if (x==0){
                      x=mfp_reg[MFPR_TBDR];
                    }else{
                      x--;
                    }
                  }
                }
              }
              LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_MFP_TIMERS,Str("MFP: ")+HEXSl(old_pc,6)+
                      " - Read timer "+char('A'+(n-MFPR_TADR))+" counter as "+x); )
            }else if (n>=MFPR_SCR){
              x=RS232_ReadReg(n);
            }else{
              x=mfp_reg[n];
            }
          }
        }else{ // Even address
          // Byte access causes bus error
          DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) if (io_word_access==0) exception(BOMBS_BUS_ERROR,EA_READ,addr);
        }
        return x;
      }else{ // max allowed address in range is 0xfffa3f
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }
      break;
    }
    case 0xff9800:        // Falcon 256 colour palette
    case 0xff9900:        // Falcon 256 colour palette
    case 0xff9a00:        // Falcon 256 colour palette
    case 0xff9b00:        // Falcon 256 colour palette
      if (emudetect_called){
        int n=(addr-0xff9800)/4;
        DWORD val=emudetect_falcon_stpal[n];
        return DWORD_B(&val,addr & 3);
      }
      exception(BOMBS_BUS_ERROR,EA_READ,addr);
      break;
    case 0xff9200:{      //----------------------------------- paddles
      bool Illegal=0;
      BYTE ret=JoyReadSTEAddress(addr,&Illegal);
      if (Illegal) exception(BOMBS_BUS_ERROR,EA_READ,addr);
      return ret;
    }case 0xff9000:{      //----------------------------------- ?
      if(addr>0xff9001){
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }break;
    }case 0xff8a00:{      //----------------------------------- Blitter
#if defined(STEVEN_SEAGAL) && defined(SS_VID_SHIFTER_EVENTS)
      BYTE b=Blitter_IO_ReadB(addr);
      VideoEvents.Add(scan_y,LINECYCLES,'b',((addr-0xff8a00)<<8)|b);
      return b;
#else
      return Blitter_IO_ReadB(addr);
#endif
    }case 0xff8900:{      //----------------------------------- STE DMA Sound
#if defined(SS_STF)
      if(ST_TYPE!=STE)
      {
#if defined(SS_STF_TRACE)
        TRACE("STF read STE DMA Sound %x\n",addr);
#endif
        //exception(BOMBS_BUS_ERROR,EA_READ,addr);
        return 0; // crash?
      break;
      }
#endif
      switch (addr){
        case 0xff8901:   //DMA control register
          LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_SOUND,Str("SOUND: ")+HEXSl(old_pc,6)+
                        " - Read DMA sound control as $"+HEXSl(dma_sound_control,2)); )
          return dma_sound_control;
        case 0xff8903:   //HiByte of frame start address
        case 0xff8905:   //MidByte of frame start address
        case 0xff8907:   //LoByte of frame start address
          return DWORD_B(&next_dma_sound_start,(0xff8907-addr)/2);
/*
  DMA-Sound Count Register:

    $FFFF8908  0 0 X X X X X X X X X X X X X X   Hibyte  (ro)
    $FFFF890A  X X X X X X X X X X X X X X X X   Midbyte (ro)
    $FFFF890C  X X X X X X X X X X X X X X X 0   Lowbyte (ro)

Used internally for the DMA-soundchip to count from start- to end-address. 
No write access.
*/
        case 0xff8909:   //HiByte of frame address counter
        case 0xff890B:   //MidByte of frame address counter
        case 0xff890D:   //LoByte of frame address counter
        {
          if (addr==0xff8909) return DWORD_B_2(&dma_sound_fetch_address);
          if (addr==0xff890B) return DWORD_B_1(&dma_sound_fetch_address);
          return DWORD_B_0(&dma_sound_fetch_address);
        }
        case 0xff890F:   //HiByte of frame end address
          return LOBYTE(HIWORD(next_dma_sound_end));
        case 0xff8911:   //MidByte of frame end address
          return HIBYTE(LOWORD(next_dma_sound_end));
        case 0xff8913:   //LoByte of frame end address
          return LOBYTE(next_dma_sound_end);
        case 0xff8921:   //Sound mode control
          return dma_sound_mode;

        case 0xff8922:          // MicroWire data hi
        case 0xff8923:          // MicroWire data lo
        case 0xff8924:          // MicroWire Mask hi
        case 0xff8925:          // MicroWire Mask lo
        {
          WORD dat=0;
          WORD mask=MicroWire_Mask;
          if (MicroWire_StartTime){
            int nShifts=DWORD(ABSOLUTE_CPU_TIME-MicroWire_StartTime)/CPU_CYCLES_PER_MW_SHIFT;
            if (nShifts>15){
              MicroWire_StartTime=0;
            }else{
              dat=WORD(MicroWire_Data << nShifts);
              while (nShifts--){
#if defined(STEVEN_SEAGAL) && defined(SS_VAR_REWRITE) 
                BOOL lobit=(mask & BIT_15)!=0;
#else
                bool lobit=(mask & BIT_15)!=0;// warning
#endif
                mask<<=1;
                mask|=lobit;
              }
            }
          }
          if (addr==0xff8922) return HIBYTE(dat);
          if (addr==0xff8923) return LOBYTE(dat);
          if (addr==0xff8924) return HIBYTE(mask);
          if (addr==0xff8925) return LOBYTE(mask);
          return 0;
        }
      }
      if (addr>0xff893f){ // FF8900 - FF893F   DMA sound, microwire
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }
      break;
    }case 0xff8800:{      //----------------------------------- sound chip
      if ((ioaccess & IOACCESS_FLAG_PSG_BUS_JAM_R)==0){
        DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) BUS_JAM_TIME(4);
        ioaccess|=IOACCESS_FLAG_PSG_BUS_JAM_R;
      }
      if ((addr & 1) && io_word_access) return 0xff; //odd addresses ignored on word writes

      if ((addr & 2)==0){ //read data / register select, mirrored at 4,8,12,...
        if (psg_reg_select==PSGR_PORT_A){
          // Drive A, drive B, side, RTS, DTR, strobe and monitor GPO
          // are normally set by ST
          BYTE Ret=psg_reg[PSGR_PORT_A];

          // Parallel port 0 joystick fire (strobe)
          if (stick[N_JOY_PARALLEL_0] & BIT_4){
            if (stick[N_JOY_PARALLEL_0] & BIT_7){
              Ret&=~BIT_5;
            }else{
              Ret|=BIT_5;
            }
          }
          return Ret;
        }else if (psg_reg_select==PSGR_PORT_B){
          if ((stick[N_JOY_PARALLEL_0] & BIT_4)==0 && (stick[N_JOY_PARALLEL_1] & BIT_4)==0){
            if (ParallelPort.IsOpen()){
              ParallelPort.NextByte();
              UpdateCentronicsBusyBit();
              return ParallelPort.ReadByte();
            }else{
              return 0xff;
            }
          }else{
            return BYTE(0xff & ~( (stick[N_JOY_PARALLEL_0] & b1111) | ((stick[N_JOY_PARALLEL_1] & b1111) << 4) ));
          }
        }else{
          return psg_reg_data;
        }
      }
      return 0xff;
    }case 0xff8600:{      //----------------------------------- DMA/FDC
/*
-------+-----+-----------------------------------------------------+----------
##############DMA/WD1772 Disk controller                           ###########
-------+-----+-----------------------------------------------------+----------
$FF8600|     |Reserved                                             |
$FF8602|     |Reserved                                             |
$FF8604|word |FDC access/sector count                              |R/W
$FF8606|word |DMA mode/status                             BIT 2 1 0|R
       |     |Condition of FDC DATA REQUEST signal -----------' | ||
       |     |0 - sector count null,1 - not null ---------------' ||
       |     |0 - no error, 1 - DMA error ------------------------'|
$FF8606|word |DMA mode/status                 BIT 8 7 6 . 4 3 2 1 .|W
       |     |0 - read FDC/HDC,1 - write ---------' | | | | | | |  |
       |     |0 - HDC access,1 - FDC access --------' | | | | | |  |
       |     |0 - DMA on,1 - no DMA ------------------' | | | | |  |
       |     |Reserved ---------------------------------' | | | |  |
       |     |0 - FDC reg,1 - sector count reg -----------' | | |  |
       |     |0 - FDC access,1 - HDC access ----------------' | |  |
       |     |0 - pin A1 low, 1 - pin A1 high ----------------' |  |
       |     |0 - pin A0 low, 1 - pin A0 high ------------------'  |
$FF8609|byte |DMA base and counter (High byte)                     |R/W
$FF860B|byte |DMA base and counter (Mid byte)                      |R/W
$FF860D|byte |DMA base and counter (Low byte)                      |R/W
-------+-----+-----------------------------------------------------+----------
*/
      if (addr>0xff860f) exception(BOMBS_BUS_ERROR,EA_READ,addr);
      if (addr<0xff8604) exception(BOMBS_BUS_ERROR,EA_READ,addr);
      if (addr<0xff8608 && io_word_access==0) exception(BOMBS_BUS_ERROR,EA_READ,addr);
#if USE_PASTI //SS pasti.dll will give us the correct byte 
      if (hPasti && pasti_active){
        if (addr<0xff8608){ // word only
          if (addr & 1) return LOBYTE(pasti_store_byte_access);
        }
        struct pastiIOINFO pioi;
        pioi.addr=addr;
        pioi.stPC=pc;
        pioi.cycles=ABSOLUTE_CPU_TIME;
//          log_to(LOGSECTION_PASTI,Str("PASTI: IO read addr=$")+HEXSl(addr,6)+" pc=$"+HEXSl(pc,6)+" cycles="+pioi.cycles);
        pasti->Io(PASTI_IOREAD,&pioi);
        pasti_handle_return(&pioi);
        if (addr<0xff8608){ // word only
          pasti_store_byte_access=WORD(pioi.data);
          pioi.data=HIBYTE(pioi.data);
        }
//          log_to(LOGSECTION_PASTI,Str("PASTI: Read returning $")+HEXSl(BYTE(pioi.data),2)+" ("+BYTE(pioi.data)+")");
        return BYTE(pioi.data);
      }
#endif
      switch(addr){
      case 0xff8604:  //high byte of FDC access
        //should check bit 8 = 0 (read)
        if (dma_mode & BIT_4){ //read sector counter (maintained by the DMA chip)
          return HIBYTE(dma_sector_count);
        }
        if (dma_mode & BIT_3){ // HD access
          LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
                " - Reading high byte of HDC register #"+((dma_mode & BIT_1) ? 1:0)); )
          return 0xff;
        }
        return 0xff;
      case 0xff8605:  //low byte of FDC access
        //should check bit 8 = 0, read
        if (dma_mode & BIT_4){ //read sector counter (maintained by the DMA chip)
          return LOBYTE(dma_sector_count);
        } 

        if (dma_mode & BIT_3){ // HD access
          LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
                  " - Reading low byte of HDC register #"+((dma_mode & BIT_1) ? 1:0)); )
          return 0xff;
        }

        // Read FDC register
#if defined(STEVEN_SEAGAL) && defined(SS_FDC_IPF)
        if(Caps.DriveIsIPF[floppy_current_drive()])
        {
          if(!(dma_mode & (BIT_1+BIT_2)))
          {
            mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
            WD1772.lineout&=~CAPSFDC_LO_INTRQ; // and in the WD1772
          }
          BYTE value=CapsFdcRead(&WD1772,(dma_mode & (BIT_1+BIT_2))/2); 
          return value;
        }
        else // else switch...
#endif
        switch (dma_mode & (BIT_1+BIT_2)){
          case 0://SS read status
/*
r0 (read) - Status Register - The value in this register depends on
the previous 177x command.  If the 177x receives a Force Interrupt
command while it is executing another command, the FDDC will clear the
Busy bit (Status bit 0: see later in this paragraph), and leave all
other Status bits unchanged.  If the 177x receives a Force Interrupt
command while it is not executing a command, the 177x will update the
entire Status Register as though it had just executed a Type I
command.  (For an explanation of Type I, Type II, Type III, and Type
IV commands, see the section on commands.)  When the CPU is connected
(directly or indirectly) to the 177x's interrupt output, it is a bad
idea to check the Busy bit because a CPU read of the Status Register
clears the 177x's interrupt output.  After the CPU writes to the
Command Register, it should not attempt to read the Busy bit for 24
cycles.  The CPU should not follow a Command Register write with a
read of Status bits 1-7 until 32 clock cycles have elapsed.

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
          {
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
              mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,true); // Turn off IRQ output
            }
//            log_DELETE_SOON(Str("FDC: ")+HEXSl(old_pc,6)+" - reading FDC status register as $"+HEXSl(fdc_str,2));
/*
            LOG_ONLY( if (mode==STEM_MODE_CPU) log_to(LOGSECTION_FDC,Str("FDC: ")+HEXSl(old_pc,6)+
                            " - Read status register as $"+HEXSl(fdc_str,2)); )
*/
            return fdc_str;
          }
          case 2:
            return fdc_tr; //track register
          case 4:
            return fdc_sr; //sector register
          case 6:
            return fdc_dr; //data register
        }
        break;
      case 0xff8606:  //high byte of DMA status
        return 0x0;
      case 0xff8607:  //low byte of DMA status
        return BYTE(b11110000) | dma_status;
      case 0xff8609:  //high byte of DMA pointer
        return (BYTE)((dma_address&0xff0000)>>16);
      case 0xff860b:  //mid byte of DMA pointer
        return (BYTE)((dma_address&0xff00)>>8);
      case 0xff860d:  //low byte of DMA pointer
        return (BYTE)((dma_address&0xff));
      case 0xff860e: //frequency/density control
      {
        if (FloppyDrive[floppy_current_drive()].STT_File) return 0;

        TFloppyImage *floppy=&(FloppyDrive[floppy_current_drive()]);
        return BYTE((floppy->BytesPerSector * floppy->SectorsPerTrack)>7000);
      }
      case 0xff860f: //high byte of frequency/density control?
        return 0;
      }
      break;
    }case 0xff8200:{      //----------------------------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
                     //----------------------------------------=--------------- shifter
/*
allowed addresses

FF8200 - FF820F   SHIFTER
FF8240 - FF827F   palette, res
*/
      if (addr>=0xff8240 && addr<0xff8260){  //palette
        int n=addr-0xff8240;n/=2;
        if (addr&1) return LOBYTE(STpal[n]);
        else return HIBYTE(STpal[n]);
      }else if (addr>0xff820f && addr<0xff8240){ //forbidden gap
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }else if (addr>0xff827f){  //forbidden area after SHIFTER
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }else{
        switch(addr){
        case 0xff8201:  //high byte of screen memory address
          return LOBYTE(HIWORD(xbios2));
        case 0xff8203:  //mid byte of screen memory address
          return HIBYTE(LOWORD(xbios2));
        case 0xff820d:  //low byte of screen memory address
#if defined(STEVEN_SEAGAL) && defined(SS_STF) && defined(SS_DEBUG)
          if(ST_TYPE!=STE) 
          {
#if defined(SS_STF_TRACE)
            TRACE("STF read 0xff820d\n");
#endif
            ASSERT(!(xbios2 & 0xFF)); // protection in iow.cpp
          }
#endif
          return LOBYTE(xbios2);
        case 0xff8205:  //high byte of screen draw pointer
        case 0xff8207:  //mid byte of screen draw pointer
        case 0xff8209:{  //low byte of screen draw pointer
          MEM_ADDRESS sdp;
#if defined(STEVEN_SEAGAL) && defined(SS_VID_SDP_READ)
          sdp=Shifter.ReadSDP(LINECYCLES); // a complicated affair
#else
          if(scan_y<shifter_first_draw_line || scan_y>=shifter_last_draw_line){
            sdp=shifter_draw_pointer;
          }else{
            sdp=get_shifter_draw_pointer(ABSOLUTE_CPU_TIME-cpu_timer_at_start_of_hbl);
            LOG_ONLY( DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) log_to(LOGSECTION_VIDEO,Str("VIDEO: ")+HEXSl(old_pc,6)+
                        " - Read shifter draw pointer as $"+HEXSl(sdp,6)+
                        " on "+scanline_cycle_log()); )
          }
#if defined(STEVEN_SEAGAL) && !defined(SS_VID_SDP_READ) && defined(SS_VID_SDP_TRACE)
          TRACE("ReadSDP y %d c %d %X->%X(%d)\n",scan_y,LINECYCLES,shifter_draw_pointer_at_start_of_line,sdp,sdp-shifter_draw_pointer_at_start_of_line);
#endif
#endif
          BYTE b=DWORD_B(&sdp,(2-(addr-0xff8205)/2)); // change for big endian !!!!!!!!!
#if defined(STEVEN_SEAGAL) && defined(SS_VID_SHIFTER_EVENTS)
          VideoEvents.Add(scan_y,LINECYCLES,'r',((addr&0xF)<<8)|b);
#endif
          return b;//DWORD_B(&sdp, (2-(addr-0xff8205)/2) );   
        }

        case 0xff820a:  //synchronization mode
          if (shifter_freq==50) return b11111110; // SS FE
          return b11111100; // SS FC - superior bits are set (in Hatari too)

        case 0xff820b:
#if defined(STEVEN_SEAGAL) && defined(SS_STF)
          if(ST_TYPE!=STE)
          {
#if defined(SS_STF_TRACE)
            TRACE("STF read 0xff820b: 0\n");
#endif
            return 0xFF; // STF; fixes Live & Let Die
          }
#endif
          return 0; // STE
          break;

        case 0xff820f:
#if defined(STEVEN_SEAGAL) && defined(SS_STF)
          if(ST_TYPE!=STE)
          {
#if defined(SS_STF_TRACE)
            TRACE("STF read 0xff820f: 0\n");
#endif
            return 0;
          }
#endif
          return (BYTE)shifter_fetch_extra_words;

        //// Unused bytes between $60 and $80 should return 0!
        case 0xff8260: //resolution
          return (BYTE)screen_res;
        case 0xff8264:  //hscroll no increase screen width
          return (BYTE)0;
        case 0xff8265:  //hscroll
          DEBUG_ONLY( if (mode==STEM_MODE_CPU) ) shifter_hscroll_extra_fetch=(shifter_hscroll!=0);
#if defined(STEVEN_SEAGAL) && defined(SS_STF)
          if(ST_TYPE!=STE)
          {
#if defined(SS_STF_TRACE)
            TRACE("STF read 0xff8265: 0 %d\n");
#endif
            return 0;
          }
#endif
          return (BYTE)shifter_hscroll;
        }
        // Below $10 - Odd bytes return value or 0, even bytes return 0xfe/0x7e
        // Above $40 - Unused return 0
        if (addr<=0xff820f && (addr & 1)==0) return 0xfe;
        return 0;
      }
      break;
    }case 0xff8000:{      //----------------------------------- MMU
      if (addr==0xff8001){
        if (mem_len>FOUR_MEGS) return MEMCONF_2MB | (MEMCONF_2MB << 2);
#if defined(SS_MMU_TRACE)
        TRACE("PC %X read MMU %X\n",pc,mmu_memory_configuration);
#endif
        return mmu_memory_configuration;
      }else if (addr>0xff800f){ //forbidden range
        exception(BOMBS_BUS_ERROR,EA_READ,addr);
      }else if (addr & 1){
        return 0;
      }
      return 0xff;
    case 0xffc100:
#ifdef DEBUG_BUILD
      if (addr==0xffc123) return (BYTE)runstate;
#endif
      if (emudetect_called){
        switch (addr){
          case 0xffc100: return BYTE(stem_version_text[0]-'0');
          case 0xffc101:
          {
            Str minor_ver=stem_version_text+2;
            for (int i=0;i<minor_ver.Length();i++){
              if (minor_ver[i]<'0' || minor_ver[i]>'9'){
                minor_ver.SetLength(i);
                break;
              }
            }
            int ver=atoi(minor_ver.RPad(2,'0'));
            return BYTE(((ver/10) << 4) | (ver % 10));
          }
          case 0xffc102: return BYTE(slow_motion);
          case 0xffc103: return BYTE(slow_motion_speed/10);
          case 0xffc104: return BYTE(fast_forward);
          case 0xffc105: return BYTE(n_cpu_cycles_per_second/1000000);
          case 0xffc106: return BYTE(0 DEBUG_ONLY(+1));
          case 0xffc107: return snapshot_loaded;
          case 0xffc108: return BYTE((100000/run_speed_ticks_per_second) >> 8);
          case 0xffc109: return BYTE((100000/run_speed_ticks_per_second) & 0xff);
          case 0xffc10a:
            if (avg_frame_time) return BYTE((((12000/avg_frame_time)*100)/shifter_freq) >> 8);
            return 0;
          case 0xffc10b:
            if (avg_frame_time) return BYTE((((12000/avg_frame_time)*100)/shifter_freq) & 0xff);
            return 0;
          case 0xffc10c: return HIBYTE(HIWORD(ABSOLUTE_CPU_TIME));
          case 0xffc10d: return LOBYTE(HIWORD(ABSOLUTE_CPU_TIME));
          case 0xffc10e: return HIBYTE(LOWORD(ABSOLUTE_CPU_TIME));
          case 0xffc10f: return LOBYTE(LOWORD(ABSOLUTE_CPU_TIME));

          case 0xffc110: return HIBYTE(HIWORD(cpu_time_of_last_vbl));
          case 0xffc111: return LOBYTE(HIWORD(cpu_time_of_last_vbl));
          case 0xffc112: return HIBYTE(LOWORD(cpu_time_of_last_vbl));
          case 0xffc113: return LOBYTE(LOWORD(cpu_time_of_last_vbl));

          case 0xffc114: return HIBYTE(HIWORD(cpu_timer_at_start_of_hbl));
          case 0xffc115: return LOBYTE(HIWORD(cpu_timer_at_start_of_hbl));
          case 0xffc116: return HIBYTE(LOWORD(cpu_timer_at_start_of_hbl));
          case 0xffc117: return LOBYTE(LOWORD(cpu_timer_at_start_of_hbl));

          case 0xffc118: return HIBYTE( (short) (scan_y));
          case 0xffc119: return LOBYTE( (short) (scan_y));
          case 0xffc11a: return emudetect_write_logs_to_printer;
          case 0xffc11b: return emudetect_falcon_mode;
          case 0xffc11c: return BYTE((emudetect_falcon_mode_size-1) + (emudetect_falcon_extra_height ? 2:0));
          case 0xffc11d: return emudetect_overscans_fixed;
        }
        if (addr<0xffc120) return 0;
      }
    }default:{ //not in allowed area
      exception(BOMBS_BUS_ERROR,EA_READ,addr);
    }       //end case
  }       //end switch
  return 0xff;
}
//---------------------------------------------------------------------------
WORD ASMCALL io_read_w(MEM_ADDRESS addr)
{
  if (addr>=0xff8240 && addr<0xff8260){  //palette
    DEBUG_CHECK_READ_IO_W(addr);
    int n=addr-0xff8240;n/=2;
    return STpal[n];
  }else{
    io_word_access=true;
    WORD x=WORD(io_read_b(addr) << 8);
    x|=io_read_b(addr+1);
    io_word_access=0;
    return x;
  }
}
//---------------------------------------------------------------------------
DWORD ASMCALL io_read_l(MEM_ADDRESS addr)
{
  INSTRUCTION_TIME(-4);
  DWORD x=io_read_w(addr) << 16;
  INSTRUCTION_TIME(4);
  x|=io_read_w(addr+2);

  return x;
}

#undef LOGSECTION
