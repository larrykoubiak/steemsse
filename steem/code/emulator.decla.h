#pragma once
#ifndef EMULATOR_DECLA_H
#define EMULATOR_DECLA_H

#include <conditions.h>
#include <dynamicarray.h>

#include <SSE/SSEVideo.h>
#include <SSE/SSEDebug.h>

#if defined(SSE_BLT_CPU_RUNNING)
#include <blitter.decla.h>
#endif

// they've been nuked in conditions -> should do without...
#define EXT extern 
#define INIT(s)

EXT int MILLISECONDS_TO_HBLS(int); 
EXT void make_Mem(BYTE,BYTE);
EXT void GetCurrentMemConf(BYTE[2]);

EXT BYTE *Mem INIT(NULL),*Rom INIT(NULL);
EXT WORD tos_version;

#define COLOUR_MONITOR (mfp_gpip_no_interrupt & MFP_GPIP_COLOUR)
#define MONO (!(mfp_gpip_no_interrupt&MFP_GPIP_COLOUR))

#define ON_RTE_RTE 0
#define ON_RTE_STEMDOS 1
#define ON_RTE_LINE_A 2
#define ON_RTE_EMHACK 3
#define ON_RTE_DONE_MALLOC_FOR_EM 4
#define ON_RTE_STOP 400
#if defined(SSE_BOILER_RUN_TO_RTS)
#define ON_RTS_STOP 401
#endif

#if defined(SSE_CPU_LINE_F)
#define ON_RTE_LINE_F 5
#endif

EXT int interrupt_depth INIT(0);

#if defined(SSE_VAR_RESIZE)
EXT WORD em_width INIT(480);
EXT WORD em_height INIT(480);
EXT BYTE em_planes INIT(4);
EXT BYTE extended_monitor INIT(0);
#else
EXT int em_width INIT(480);
EXT int em_height INIT(480);
EXT int em_planes INIT(4);
EXT int extended_monitor INIT(0);//SS can't be bool
#endif
EXT DWORD n_cpu_cycles_per_second INIT(8000000),new_n_cpu_cycles_per_second INIT(0),n_millions_cycles_per_sec INIT(8);

#if defined(SSE_TIMING_MULTIPLIER_392)
#ifdef SSE_BUGFIX_394
extern "C" double cpu_cycles_multiplier INIT(1.0);
#else
EXT double cpu_cycles_multiplier INIT(1.0);
#endif
#endif

EXT int on_rte;
EXT int on_rte_interrupt_depth;

extern "C"
{
EXT MEM_ADDRESS shifter_draw_pointer;
#if defined(SSE_VAR_RESIZE)
EXT BYTE shifter_hscroll, shifter_skip_raster_for_hscroll; // the latter bool
#else
EXT int shifter_hscroll,shifter_skip_raster_for_hscroll;
#endif
}

EXT MEM_ADDRESS xbios2,shifter_draw_pointer_at_start_of_line;
EXT int shifter_pixel;

#if defined(SSE_VAR_RESIZE)
EXT BYTE shifter_freq INIT(60);
#else
EXT int shifter_freq INIT(60);
#endif

#if defined(SSE_VAR_RESIZE)
EXT BYTE shifter_freq_idx INIT(1); //1 for 60hz
#else
EXT int shifter_freq_idx INIT(1);
#endif
EXT int shifter_x,shifter_y;
EXT int shifter_first_draw_line;
EXT int shifter_last_draw_line;
EXT int shifter_scanline_width_in_bytes;
#if defined(SSE_VAR_RESIZE)
EXT BYTE shifter_fetch_extra_words;
#else
EXT int shifter_fetch_extra_words;
#endif
EXT bool shifter_hscroll_extra_fetch;
#if defined(SSE_VAR_RESIZE_392)
EXT BYTE screen_res INIT(0);
EXT short scan_y;
#else
EXT int screen_res INIT(0);
EXT int scan_y;
#endif
#define SVmemvalid 0x420
#define SVmemctrl 0x424
#define SVphystop 0x42e
#define SV_membot 0x432
#define SV_memtop 0x436
#define SVmemval2 0x43a
#define SVscreenpt 0x45e

#define SVsshiftmd 0x44c
#define SV_v_bas_ad 0x44e

#define SV_drvbits 0x4c2

#define MEMCONF_128 0
#define MEMCONF_512 1
#define MEMCONF_2MB 2

// The MMU config never gets set to this, but we use it to distinguish between
// 640Kb (retained for legacy reasons) and 512Kb.
#define MEMCONF_0   3
#define MEMCONF_512K_BANK1_CONF MEMCONF_0
#define MEMCONF_2MB_BANK1_CONF MEMCONF_0

#define MEMCONF_7MB 4  
#if defined(SSE_MMU_MONSTER_ALT_RAM)
#define MEMCONF_6MB 5
#endif

#define MB2 (2*1024*1024)
#define KB512 (512*1024)
#define KB128 (128*1024)

EXT MEM_ADDRESS mmu_confused_address(MEM_ADDRESS ad);
extern "C"{
BYTE ASMCALL mmu_confused_peek(MEM_ADDRESS ad,bool cause_exception);
WORD ASMCALL mmu_confused_dpeek(MEM_ADDRESS ad,bool cause_exception);
LONG ASMCALL mmu_confused_lpeek(MEM_ADDRESS ad,bool cause_exception);
void ASMCALL mmu_confused_set_dest_to_addr(int bytes,bool cause_exception);
}

EXT BYTE mmu_memory_configuration;

EXT MEM_ADDRESS mmu_bank_length[2];
EXT MEM_ADDRESS bank_length[2];

#if defined(SSE_MMU_MONSTER_ALT_RAM)
extern const MEM_ADDRESS mmu_bank_length_from_config[6];
#else
extern const MEM_ADDRESS mmu_bank_length_from_config[5];
#endif

extern void intercept_os();

extern void call_a000();

WIN_ONLY( EXT CRITICAL_SECTION agenda_cs; )

EXT void ASMCALL emudetect_falcon_draw_scanline(int,int,int,int);
EXT void emudetect_falcon_palette_convert(int);
EXT void emudetect_init();
//---------------------------------------------------------------------------
                  
void init_timings();

#define OS_CALL 0
#define OS_NO_CALL true

EXT MEM_ADDRESS os_gemdos_vector,os_bios_vector,os_xbios_vector;
void intercept_gemdos(),intercept_bios(),intercept_xbios();

void emudetect_reset();
#if defined(SSE_VAR_NO_EMU_DETECT)
const EXT bool emudetect_called,emudetect_write_logs_to_printer,
  emudetect_overscans_fixed;
#else
EXT bool emudetect_called,emudetect_write_logs_to_printer,
  emudetect_overscans_fixed;
#endif
#define EMUD_FALC_MODE_OFF 0
#define EMUD_FALC_MODE_8BIT 1
#define EMUD_FALC_MODE_16BIT 2

EXT BYTE emudetect_falcon_mode;
EXT BYTE emudetect_falcon_mode_size;
EXT bool emudetect_falcon_extra_height;

EXT DynamicArray<DWORD> emudetect_falcon_stpal;
EXT DynamicArray<DWORD> emudetect_falcon_pcpal;

EXT BYTE snapshot_loaded;

#ifndef NO_CRAZY_MONITOR
void extended_monitor_hack();
#endif

EXT bool vbl_pending;

#define MAX_AGENDA_LENGTH 32

typedef void AGENDAPROC(int);
typedef AGENDAPROC* LPAGENDAPROC;

struct AGENDA_STRUCT{ // SS removed _
  LPAGENDAPROC perform;
  unsigned long time;
  int param;
};

EXT AGENDA_STRUCT agenda[MAX_AGENDA_LENGTH];
EXT int agenda_length;
EXT unsigned long agenda_next_time;

void agenda_add(LPAGENDAPROC,int,int);
void agenda_delete(LPAGENDAPROC);

void agenda_keyboard_replace(int);

int agenda_get_queue_pos(LPAGENDAPROC);
//void inline agenda_process();
void agenda_acia_tx_delay_IKBD(int),agenda_acia_tx_delay_MIDI(int);


EXT MEM_ADDRESS on_rte_return_address;


#ifndef SSE_CPU

#define M68K_UNSTOP                         \
  if (cpu_stopped){ \
                   \
                  cpu_stopped=false;     \
                  SET_PC((pc+4) | pc_high_byte);          \
  }
#endif//cpu

// This list is used to reinit the agendas after loading a snapshot
// add any new agendas to the end of the list, replace old agendas
// with NULL.
//EXT LPAGENDAPROC agenda_list[]; // BCC don't like that
EXT LPAGENDAPROC agenda_list[
  15+4+1
  +1
#if defined(SSE_FDC_VERIFY_AGENDA)
  +1
#endif
];
//--------------------------------------------------------------------------- SHIFTER

//#define SHIFTER_DRAWING_NOT 0
//#define SHIFTER_DRAWING_PICTURE 1
//#define SHIFTER_DRAWING_BORDER -1
//#define SHIFTER_DRAWING_PICTURE_EXCEPT_NOT 2
// >0 must indicate timer B running

//int shifter_drawing;

//int scanline_raster_position();

//int shifter_scanline_width_in_bytes_from_res[3]={160,160,80};
//--------------------------------------------------------------------------- ACIAs
int ACIAClockToHBLS(int,bool=0);
void ACIA_Reset(int,bool);
void ACIA_SetControl(int,BYTE);

#if !(defined(SSE_ACIA)) //see new file acia.decla.h
struct _ACIA_STRUCT{
  int clock_divide;

  int rx_delay__unused;
  bool rx_irq_enabled;
  bool rx_not_read;

  int overrun;

  int tx_flag;
  bool tx_irq_enabled;

  BYTE data;
  bool irq;

  int last_tx_write_time;
  int last_rx_read_time;
};

EXT struct _ACIA_STRUCT acia[2];

#define ACIA_OVERRUN_NO 0
#define ACIA_OVERRUN_COMING 1
#define ACIA_OVERRUN_YES 416
#define NUM_ACIA_IKBD 0
#define NUM_ACIA_MIDI 1
#define ACIA_CYCLES_NEEDED_TO_START_TX 512

#define ACIA_IKBD acia[0]
#define ACIA_MIDI acia[1]


#endif



#ifndef NO_CRAZY_MONITOR

EXT int aes_calls_since_reset;
EXT long save_r[16];

EXT MEM_ADDRESS line_a_base;
EXT MEM_ADDRESS vdi_intout;

#endif

#undef EXT
#undef INIT


#endif// EMULATOR_DECLA_H
