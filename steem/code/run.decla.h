#pragma once
#ifndef RUN_DECLA_H
#define RUN_DECLA_H

#include "SSE/SSEDecla.h"

#define EXT extern
#define INIT(s)

#define RUNSTATE_RUNNING 0
#define RUNSTATE_STOPPING 1
#define RUNSTATE_STOPPED 2

#define ABSOLUTE_CPU_TIME (cpu_timer-cpu_cycles)

EXT void run();
EXT void prepare_cpu_boosted_event_plans();

#if defined(SSE_VAR_RESIZE)
EXT BYTE runstate;
#else
EXT int runstate;
#endif
// fast_forward_max_speed=(1000 / (max %/100)); 0 for unlimited
EXT int fast_forward INIT(0),fast_forward_max_speed INIT(0);
EXT bool fast_forward_stuck_down INIT(0);
EXT int slow_motion INIT(0),slow_motion_speed INIT(100);
#if defined(SSE_NO_RUN_SPEED)
const EXT int run_speed_ticks_per_second;
#else
EXT int run_speed_ticks_per_second INIT(1000);
#endif
EXT bool disable_speed_limiting INIT(0);
LOG_ONLY( EXT int run_start_time; )
UNIX_ONLY( EXT bool RunWhenStop INIT(0); )

EXT DWORD avg_frame_time INIT(0),avg_frame_time_timer,frame_delay_timeout,timer;
EXT DWORD speed_limit_wait_till;
EXT int avg_frame_time_counter INIT(0);
EXT DWORD auto_frameskip_target_time;
#define AUTO_FRAMESKIP 8
#if defined(SSE_VID_ENFORCE_AUTOFRAMESKIP) && !defined(SSE_BUGFIX_393)
EXT const BYTE frameskip INIT(AUTO_FRAMESKIP);
EXT BYTE frameskip_count INIT(1);
#elif defined(SSE_VAR_RESIZE) && !defined(SSE_BUGFIX_393)
EXT BYTE frameskip INIT(AUTO_FRAMESKIP);
EXT BYTE frameskip_count INIT(1);
#else
EXT int frameskip INIT(AUTO_FRAMESKIP);
EXT int frameskip_count INIT(1);
#endif


EXT bool flashlight_flag INIT(false);
#ifdef DEBUG_BUILD
#define SET_WHY_STOP(s) runstate_why_stop=s;
#else
#define SET_WHY_STOP(s)
#endif

DEBUG_ONLY(EXT int mode);

#if defined(SSE_VAR_RESIZE)
EXT BYTE mixed_output INIT(0);
#else
EXT int mixed_output INIT(0);
#endif

#if defined(SSE_TIMINGS_CPUTIMER64)
EXT COUNTER_VAR cpu_time_of_last_vbl,shifter_cycle_base;
EXT COUNTER_VAR cpu_timer_at_start_of_hbl;
#else
EXT int cpu_time_of_last_vbl,shifter_cycle_base;
EXT int cpu_timer_at_start_of_hbl;
#endif

//#ifdef IN_EMU
#if defined(SSE_BUGFIX_394)
#define CYCLES_FROM_START_OF_HBL_IRQ_TO_WHEN_PEND_IS_CLEARED (28-10)
#else
#define CYCLES_FROM_START_OF_HBL_IRQ_TO_WHEN_PEND_IS_CLEARED 28
#endif
//          INSTRUCTION_TIME(8-((ABSOLUTE_CPU_TIME-shifter_cycle_base) % 12));


#if defined(SSE_MMU_ROUNDING_BUS)

#define INTERRUPT_START_TIME_WOBBLE  \
          cpu_cycles&=-4; \
          INSTRUCTION_TIME((8000000-(ABSOLUTE_CPU_TIME-shifter_cycle_base)) % 10);

#else

#define INTERRUPT_START_TIME_WOBBLE  \
          INSTRUCTION_TIME_ROUND(0); \
          INSTRUCTION_TIME((8000000-(ABSOLUTE_CPU_TIME-shifter_cycle_base)) % 10);

#endif

// see SSEInterrupt.cpp
#if !defined(SSE_INT_HBL)
#define HBL_INTERRUPT  \
  {                  \
    hbl_pending=false;                 \
    log_to_section(LOGSECTION_INTERRUPTS,Str("INTERRUPT: HBL at PC=")+HEXSl(pc,6)+" "+scanline_cycle_log()); \
    M68K_UNSTOP;                                \
    INTERRUPT_START_TIME_WOBBLE;                 \
    time_of_last_hbl_interrupt=ABSOLUTE_CPU_TIME; \
    INSTRUCTION_TIME_ROUND(56);                 \
    m68k_interrupt(LPEEK(0x0068));              \
    sr=(sr & WORD(~SR_IPL)) | WORD(SR_IPL_2);   \
    debug_check_break_on_irq(BREAK_IRQ_HBL_IDX);    \
  }
#endif

#if !defined(SSE_INT_VBL)
#define VBL_INTERRUPT                                                        \
          {                                                               \
            vbl_pending=false;                                             \
            log_to_section(LOGSECTION_INTERRUPTS,EasyStr("INTERRUPT: VBL at PC=")+HEXSl(pc,6)+" time is "+ABSOLUTE_CPU_TIME+" ("+(ABSOLUTE_CPU_TIME-cpu_time_of_last_vbl)+" cycles into screen)");\
            M68K_UNSTOP;                                                  \
            INTERRUPT_START_TIME_WOBBLE;                \
            INSTRUCTION_TIME_ROUND(56); \
            m68k_interrupt(LPEEK(0x0070));                                \
            sr=(sr&WORD(~SR_IPL))|WORD(SR_IPL_4);                         \
            debug_check_break_on_irq(BREAK_IRQ_VBL_IDX);    \
          }
#endif

/*  SS because agenda have an absolute timing (hbl count)
    there's no need to delete them
*/

#define CHECK_AGENDA                                              \
  if ((hbl_count++)==agenda_next_time){                           \
    if (agenda_length){                                           \
      WIN_ONLY( EnterCriticalSection(&agenda_cs); )               \
      dbg_log(EasyStr("TASKS: Executing agenda action at ")+hbl_count);      \
      if (agenda_length){                                         \
        while ((signed int)(hbl_count-agenda[agenda_length-1].time)>=0){ \
          agenda_length--;                                        \
          if (agenda[agenda_length].perform!=NULL) agenda[agenda_length].perform(agenda[agenda_length].param); \
          if (agenda_length){                                     \
            agenda_next_time=agenda[agenda_length-1].time;        \
          }else{                                                  \
            agenda_next_time=hbl_count-1; /*wait 42 hours*/       \
            break;                                                \
          }                                                       \
        }                                                         \
      }                                                           \
      WIN_ONLY( LeaveCriticalSection(&agenda_cs); )               \
    }                                                             \
  }

#define CALC_SHIFTER_FREQ_IDX           \
            switch(shifter_freq){        \
              case 50:      shifter_freq_idx=0; break;  \
              case 60:      shifter_freq_idx=1; break;   \
              default:      shifter_freq_idx=2;   \
            }

#if defined(SSE_INT_MFP_TIMERS_RUN_IF_DISABLED)
//no enabled check
#define PREPARE_EVENT_CHECK_FOR_TIMER_TIMEOUTS(tn)      \
    {                           \
      if ((time_of_next_event-mfp_timer_timeout[tn]) >= 0){  \
        time_of_next_event=mfp_timer_timeout[tn];          \
        screen_event_vector=event_mfp_timer_timeout[tn];    \
      }                                                     \
    }


#else

#define PREPARE_EVENT_CHECK_FOR_TIMER_TIMEOUTS(tn)      \
    if (mfp_timer_enabled[tn] || mfp_timer_period_change[tn]){                           \
      if ((time_of_next_event-mfp_timer_timeout[tn]) >= 0){  \
        time_of_next_event=mfp_timer_timeout[tn];          \
        screen_event_vector=event_mfp_timer_timeout[tn];    \
      }                                                     \
    }

#endif

/*#define PREPARE_EVENT_CHECK_FOR_DMA_SOUND_END  \
    if ((time_of_next_event-dma_sound_end_cpu_time) >= 0){                 \
      time_of_next_event=dma_sound_end_cpu_time;     \
      screen_event_vector=event_dma_sound_hit_end;                    \
    }                                    \
*/
#define PREPARE_EVENT_CHECK_FOR_TIMER_B       \
  if (mfp_reg[MFPR_TBCR]==8){  \
    if ((time_of_next_event-time_of_next_timer_b) >= 0){                 \
      time_of_next_event=time_of_next_timer_b;     \
      screen_event_vector=event_timer_b;                    \
    }                                    \
  }

#ifdef DEBUG_BUILD

#define PREPARE_EVENT_CHECK_FOR_DEBUG       \
  if (debug_run_until==DRU_CYCLE){    \
    if ((time_of_next_event-debug_run_until_val) >= 0){                 \
      time_of_next_event=debug_run_until_val;  \
      screen_event_vector=event_debug_stop;                    \
    }    \
  }

#else
#define PREPARE_EVENT_CHECK_FOR_DEBUG
#endif

#if USE_PASTI

#define PREPARE_EVENT_CHECK_FOR_PASTI       \
  if ((time_of_next_event-pasti_update_time) >= 0){                 \
    time_of_next_event=pasti_update_time;  \
    screen_event_vector=event_pasti_update;                    \
  }

#else
#define PREPARE_EVENT_CHECK_FOR_PASTI
#endif


#if defined(SSE_WD1772_EMU)

#define PREPARE_EVENT_CHECK_FOR_FLOPPY       \
  if ((time_of_next_event-WD1772.update_time) >= 0){                 \
    time_of_next_event=WD1772.update_time;  \
    screen_event_vector=event_wd1772;                    \
  }\
  else if ((time_of_next_event-SF314[0].time_of_next_ip) >= 0){                 \
    time_of_next_event=SF314[0].time_of_next_ip;  \
    screen_event_vector=event_driveA_ip;                    \
  }\
  else if ((time_of_next_event-SF314[1].time_of_next_ip) >= 0){                 \
    time_of_next_event=SF314[1].time_of_next_ip;  \
    screen_event_vector=event_driveB_ip;                    \
  }

#else
#define PREPARE_EVENT_CHECK_FOR_FLOPPY
#endif



typedef void(*EVENTPROC)();

#if defined(SSE_COMPILER_STRUCT_391)

#pragma pack(push, STRUCTURE_ALIGNMENT)

#if defined(SSE_TIMINGS_CPUTIMER64)
typedef struct{
  EVENTPROC event;
  COUNTER_VAR time;
}screen_event_struct;
#else
typedef struct{
  EVENTPROC event;
  int time;
}screen_event_struct;
#endif

#pragma pack(pop, STRUCTURE_ALIGNMENT)

#else

typedef struct{
  int time;
  EVENTPROC event;
}screen_event_struct;

#endif//#if defined(SSE_COMPILER_STRUCT_391)




#if defined(SSE_GLUE)
void event_trigger_vbi();
#define screen_event_pointer (&Glue.screen_event)
#else
EXT screen_event_struct event_plan_50hz[313*2+2],event_plan_60hz[263*2+2],event_plan_70hz[600*2+2],
                    event_plan_boosted_50hz[313*2+2],event_plan_boosted_60hz[263*2+2],event_plan_boosted_70hz[600*2+2];
EXT screen_event_struct*screen_event_pointer,*event_plan[4],*event_plan_boosted[4];
#endif
void prepare_next_event();
void inline prepare_event_again();
void event_timer_a_timeout();
void event_timer_b_timeout();
void event_timer_c_timeout();
void event_timer_d_timeout();
void event_scanline();
void event_timer_b();


void event_start_vbl();
void event_vbl_interrupt();
void event_hbl(); //just HBL, don't draw yet, don't increase scan_y
#if !defined(SSE_VAR_DEAD_CODE)
void event_scanline_last_line_of_60Hz(),event_scanline_last_line_of_70Hz();
#endif
EXT EVENTPROC event_mfp_timer_timeout[4];

#if defined(SSE_TIMINGS_CPUTIMER64)
EXT COUNTER_VAR time_of_next_event;
EXT COUNTER_VAR cpu_time_of_start_of_event_plan;
//int cpu_time_of_next_hbl_interrupt=0;
EXT COUNTER_VAR time_of_next_timer_b;
EXT COUNTER_VAR time_of_last_hbl_interrupt;
#if defined(SSE_INT_VBL_IACK)
EXT COUNTER_VAR time_of_last_vbl_interrupt;
#endif
EXT COUNTER_VAR cpu_timer_at_res_change;
#else
EXT int time_of_next_event;
EXT int cpu_time_of_start_of_event_plan;
//int cpu_time_of_next_hbl_interrupt=0;
EXT int time_of_next_timer_b;
EXT int time_of_last_hbl_interrupt;
#if defined(SSE_INT_VBL_IACK)
EXT int time_of_last_vbl_interrupt;
#endif
EXT int cpu_timer_at_res_change;
#endif

EXT EVENTPROC screen_event_vector;

#if defined(SSE_VAR_RESIZE)
EXT BYTE screen_res_at_start_of_vbl;
EXT BYTE shifter_freq_at_start_of_vbl;
#else
EXT int screen_res_at_start_of_vbl;
EXT int shifter_freq_at_start_of_vbl;
#endif
EXT int scanline_time_in_cpu_cycles_at_start_of_vbl;
EXT bool hbl_pending;



#ifdef DEBUG_BUILD
#define CHECK_BREAKPOINT                     \
        if (debug_num_bk){ \
          if (debug_first_instruction==0) breakpoint_check();     \
        }   \
        if (pc==trace_over_breakpoint){ \
          if (runstate==RUNSTATE_RUNNING) runstate=RUNSTATE_STOPPING;                 \
        } 
#else
#define CHECK_BREAKPOINT
#endif

//#endif

#if USE_PASTI
void event_pasti_update();
#endif

#if defined(SSE_WD1772_EMU)
/*  1 event for FDC: various parts of its program
    1 event for each drive: IP
*/
void event_wd1772();
void event_driveA_ip();
void event_driveB_ip();
#endif

#if defined(SSE_ACIA)

extern COUNTER_VAR time_of_event_acia;
void event_acia();

#define PREPARE_EVENT_CHECK_FOR_ACIA \
  if(OPTION_C1 && time_of_next_event-time_of_event_acia>=0)\
  {\
    time_of_next_event=time_of_event_acia;\
    screen_event_vector=event_acia;\
  }

#endif//acia

#if defined(SSE_INT_MFP_LATCH_DELAY)
/*  v3.8
    We create an event for write to MFP registers, it adds load but the
    alternative is too complicated (previous versions).
    The event triggers MFP_WRITE_LATENCY cycles later, which
    practically means after the next instruction, or during
    the IACK cycle.
*/

void event_mfp_write();
extern COUNTER_VAR time_of_event_mfp_write; //temp?

#define PREPARE_EVENT_CHECK_FOR_MFP_WRITE       \
  if ((time_of_next_event-time_of_event_mfp_write) >= 0){                 \
    time_of_next_event=time_of_event_mfp_write;  \
    screen_event_vector=event_mfp_write;                    \
  } 

#endif//mfp

#undef EXT
#undef INIT

#endif//RUN_DECLA_H
