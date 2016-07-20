#pragma once
#ifndef IKBD_DECLA_H
#define IKBD_DECLA_H

#include "SSE/SSEParameters.h"
#include "run.decla.h" //complicates matters...
#define EXT extern
#define INIT(s)

#include "acia.decla.h"
//EXT ACIA_STRUCT acia[2];


/*
               The Atari Intelligent Keyboard (ikbd) transmits encoded
          make/break   key   scan   codes  (with  two  key  rollover),
          mouse/trackball data, joystick data, and time of  day.   The
          ikbd  receives commands as well, with bidirectional communi-
          cation controlled on the ST side by an  MC6850  Asynchronous
          Communications  Interface Adapter supplied with transmit and
          receive clock inputs of 500 KHz.  The data transfer rate  is
          a constant 7812.5 bits/sec which can be generated by setting
          the ACIA Counter Divide Select to divide by  64.   All  ikbd
          functions  such  as  key  scanning,  mouse tracking, command
          parsing, etc. are performed by a 1 MHz HD6301V1 8 bit Micro-
          computer Unit.

               The ikbd is equipped with a combination  mouse/joystick
          port  and  a joystick only port.  The Atari Two Button Mouse
          is a mechanical, opto-mechanical, or optical mouse with  the
          following  minimal performance characteristics: a resolution
          of 100 counts/inch (4 counts/mm), a maximum velocity  of  10
          inches/second  (250  mm/second),  and  a maximum pulse phase
          error of 50 percent.  The Atari Joystick is  a  4  direction

          switch-type  joystick  with  one  fire button.  The ikbd can
          report movement using one  of  three  mouse/joystick  modes:
          mouse with joystick, disabled mouse with joystick, and joys-
          tick with joystick.


          ----- Mouse/Joystick0 Port Pin Assignments ---------------

             ST           DB 9P
                          ----                                    ----
          IKBD MATRIX       1 |<--- XB Pulse / Up Switch --------|
          IKBD MATRIX       2 |<--- XA Pulse / Down Switch ------|
          IKBD MATRIX       3 |<--- YA Pulse / Left Switch ------|
          IKBD MATRIX       4 |<--- YB Pulse / Right Switch -----|
          IKBD MCU          6 |<--- Left Button / Fire Button ---|
                            7 |---- Power ---------------------->|
                            8 |---- Ground ----------------------|
          IKBD MCU          9 |<--- Right Button / Joy1 Fire ----|
                          ----                                    ----



          Signal Characteristics

                  mouse pins 1-4          TTL levels.
                  joystick0 pins 1-4      TTL levels.
                  pin 6                   TTL levels, closure to ground.
                  pin 7                   +5 VDC.
                  pin 9                   TTL levels, closure to ground.



          Mouse Phase Directions

                    POSITIVE RIGHT (UP)             NEGATIVE LEFT (DOWN)

                  .    -----       -----                  -----       -----
          XA (YA)     |     |     |     |                |     |     |     |
                      |     |     |     |                |     |     |     |
                  . --       -----       -----      -----       -----       --

                  .       -----       -----            -----       -----
          XB (YB)        |     |     |     |          |     |     |     |
                         |     |     |     |          |     |     |     |
                  . -----       -----       --      --       -----       -----

          ----- Joystick1 Port Pin Assignments ---------------

             ST           DB 9P
                          ----                                    ----
          IKBD MATRIX       1 |<--- Up Switch -------------------|
          IKBD MATRIX       2 |<--- Down Switch -----------------|
          IKBD MATRIX       3 |<--- Left Switch -----------------|
          IKBD MATRIX       4 |<--- Right Switch ----------------|
          IKBD MCU          6 |<--- Fire Button -----------------|
                            7 |---- Power ---------------------->|
                            8 |---- Ground ----------------------|
                          ----                                    ----



          Signal Characteristics

                  pins 1-4,6              TTL levels.
                  pin 7                   +5 VDC.


*/

extern BYTE key_table[256];

#define IKBD_HBLS_FROM_COMMAND_WRITE_TO_PROCESS 5
#define IKBD_DEFAULT_MOUSE_MOVE_MAX 15 // 5 for X-Out
#define IKBD_RESET_MESSAGE 0xf1  // 0xf0 in docs
/*
If the controller self-test completes without error, the code 0xF0 is returned.
(This code will be used to indicate the version/rlease of the ikbd controller. 
The first release of the ikbd is version 0xF0, should there be a second
release it will be 0xF1, and so on.)
SS: most ST's had a 6301 that sent '$F1'
*/

#if defined(SSE_IKBD_FAKE_CUSTOM)
extern "C" void keyboard_buffer_write(BYTE,int signal=0);
#elif defined(SSE_IKBD_6301)
extern "C" void keyboard_buffer_write(BYTE);
#else
EXT void keyboard_buffer_write(BYTE);
#endif
#if defined(SSE_IKBD_6301_380)
extern "C" void keyboard_buffer_write_n_record(BYTE);
#else
EXT void keyboard_buffer_write_n_record(BYTE);
#endif
EXT void keyboard_buffer_write_string(int s1,...);
EXT bool ikbd_keys_disabled();
EXT void ikbd_mouse_move(int,int,int,int DEFVAL(IKBD_DEFAULT_MOUSE_MOVE_MAX));
#if defined(SSE_IKBD_6301)
//extern "C" int ST_Key_Down[128];
#else
EXT bool ST_Key_Down[128];
#endif
EXT int disable_input_vbl_count INIT(0);
EXT int ikbd_joy_poll_line INIT(0),ikbd_mouse_poll_line INIT(0),ikbd_key_poll_line INIT(0);

#define MAX_KEYBOARD_BUFFER_SIZE 1024

EXT BYTE keyboard_buffer[MAX_KEYBOARD_BUFFER_SIZE];
EXT int keyboard_buffer_length;

EXT int mouse_speed INIT(10);

#ifdef WIN32
// When task switching is turned off we have to manually update these keys at the VBL
extern BYTE TaskSwitchVKList[4];
extern bool CutTaskSwitchVKDown[4];
#endif

#define STKEY_PAD_DIVIDE 0x65
#define STKEY_PAD_ENTER 0x72

EXT int mouse_move_since_last_interrupt_x,mouse_move_since_last_interrupt_y;
EXT bool mouse_change_since_last_interrupt;
#if !(defined(SSE_IKBD_6301))
EXT int mousek;
#endif
#ifndef CYGWIN
#if defined(SSE_VAR_RESIZE_382)
EXT BYTE no_set_cursor_pos INIT(0);
#else
EXT int no_set_cursor_pos INIT(0);
#endif
#else
#if defined(SSE_VAR_RESIZE_382)
EXT BYTE no_set_cursor_pos INIT(true);
#else
EXT int no_set_cursor_pos INIT(true);
#endif
#endif

//#ifdef IN_EMU

/*
SCAN CODES (looks great in VC6 IDE with tab=2)

01	Esc	1B	]	            35	/		          4F	{ NOT USED }
02	1 	1C	RET	          36	(RIGHT) SHIFT	50	DOWN ARROW
03	2	  1D	CTRL	        37	{ NOT USED }	51	{ NOT USED }
04	3 	1E	A	            38	ALT		        52	INSERT
05	4 	1F	S	            39	SPACE BAR	    53	DEL
06	5 	20	D	            3A	CAPS LOCK	    54	{ NOT USED }
07	6 	21	F	            3B	F1		        5F	{ NOT USED }
08	7 	22	G	            3C	F2		        60	ISO KEY
09	8	  23	H	            3D	F3		        61	UNDO
0A	9	  24	J	            3E	F4		        62	HELP
0B	0	  25	K	            3F	F5		        63	KEYPAD (
0C	-	  26	L	            40	F6		        64	KEYPAD /
0D	==	27	;	            41	F7		        65	KEYPAD *
0E	BS	28	'	            42	F8		        66	KEYPAD *
0F	TAB	29	`	            43	F9		        67	KEYPAD 7
10	Q	  2A	(LEFT) SHIFT	44	F10	          68	KEYPAD 8
11	W	  2B	\	            45	{ NOT USED }	69	KEYPAD 9
12	E	  2C	Z	            46	{ NOT USED }	6A	KEYPAD 4
13	R	  2D	X	            47	HOME		      6B	KEYPAD 5
14	T	  2E	C	            48	UP ARROW	    6C	KEYPAD 6
15	Y	  2F	V	            49	{ NOT USED }	6D	KEYPAD 1
16	U	  30	B	            4A	KEYPAD -	    6E	KEYPAD 2
17	I	  31	N	            4B	LEFT ARROW	  6F	KEYPAD 3
18	O	  32	M	            4C	{ NOT USED }	70	KEYPAD 0
19	P	  33	,	            4D	RIGHT ARROW	  71	KEYPAD .
1A	[	  34	.	            4E	KEYPAD +	    72	KEYPAD ENTER
*/



void IKBD_VBL();
void agenda_ikbd_process(int);  //intelligent keyboard handle byte
void ikbd_run_start(bool);
void ikbd_reset(bool);

void agenda_keyboard_reset(int);
void ikbd_report_abs_mouse(int);
void ikbd_send_joystick_message(int);


#define IKBD_MOUSE_MODE_ABSOLUTE 0x9
#define IKBD_MOUSE_MODE_RELATIVE 0x8
#define IKBD_MOUSE_MODE_CURSOR_KEYS 0xa
#define IKBD_MOUSE_MODE_OFF 0x12
#define IKBD_JOY_MODE_OFF 0x1a
#define IKBD_JOY_MODE_ASK 0x15
#define IKBD_JOY_MODE_AUTO_NOTIFY 0x14
#define IKBD_JOY_MODE_CURSOR_KEYS 0x19
#define IKBD_JOY_MODE_DURATION 100
#define IKBD_JOY_MODE_FIRE_BUTTON_DURATION 101
#define BIT_RMB BIT_0
#define BIT_LMB BIT_1



#if defined(SSE_IKBD_FAKE_CUSTOM) // fixes Drag/Unlimited Bobs
#define IKBD_SCANLINES_FROM_ABS_MOUSE_POLL_TO_SEND int((MONO) ? 50:60)
#else
#define IKBD_SCANLINES_FROM_ABS_MOUSE_POLL_TO_SEND int((MONO) ? 50:30)
#endif


#define IKBD_SCANLINES_FROM_JOY_POLL_TO_SEND int((MONO) ? 32:20)   // 32:20
#define RMB_DOWN(mk) (mk & 1)
#define LMB_DOWN(mk) (mk & 2)


EXT const int ikbd_clock_days_in_mon[13];
EXT const int ikbd_clock_max_val[6];

struct IKBD_STRUCT{ // SS removed _
  int command_read_count,command_parameter_counter;
  BYTE command;
  BYTE command_param[8];
  BYTE mouse_button_press_what_message;
  int mouse_mode;
  int joy_mode;
  int abs_mouse_max_x,abs_mouse_max_y;
  int cursor_key_mouse_pulse_count_x,cursor_key_mouse_pulse_count_y;
  int relative_mouse_threshold_x,relative_mouse_threshold_y;
  int abs_mouse_scale_x,abs_mouse_scale_y;
  int abs_mouse_x,abs_mouse_y;
  bool mouse_upside_down;
  bool send_nothing;
  int duration;
  BYTE clock[6];
  DWORD cursor_key_joy_time[6];
  DWORD cursor_key_joy_ticks[4];
/*
  After any joystick command, the IKBD assumes that joysticks are connected to
 both Joystick0 and Joystick1. Any mouse command (except MOUSE DISABLE) then 
causes port 0 to again be scanned as if it were a mouse, and both buttons are 
logically connected to it. If a mouse disable command is received while port 0
 is presumed to be a mouse, the button is logically assigned to Joystick1 
( until the mouse is re enabled by another mouse command).
*/
  bool port_0_joy;
  int abs_mousek_flags;
  bool resetting;
  int psyg_hack_stage;
  int clock_vbl_count;
  WORD load_memory_address;
  BYTE ram[128]; 
  int reset_121A_hack;
  int reset_0814_hack;
  int reset_1214_hack;
  int joy_packet_pos;
  int mouse_packet_pos;

/*  WRT memory snapshots we can safely modify the structures, the size is 
    recorded along with data
*/
#if defined(SSE_ACIA_IRQ_DELAY)  // not defined anymore (v3.5.2), see MFP
  int timer_when_keyboard_info; // to improve accuracy of keyboard IRQ timing
#endif
#if defined(SSE_IKBD_POLL_IN_FRAME)
  int scanline_to_poll; // each VBL, we poll IKBD during a random scanline
#endif
#if defined(SSE_IKBD_FAKE_CUSTOM)
  int custom_prg_checksum; // for 6301 custom programs ID (boot & main)
  BOOL custom_prg_loading; // faking loader execution
  int custom_prg_ID; 
  void CustomDragonnels();
  void CustomFroggies();
  void CustomTB2();
#endif


};

EXT IKBD_STRUCT ikbd;

#if defined(SSE_IKBD_FAKE_CUSTOM)
#define IKBD_CUSTOM_DUMMY 1 
#define IKBD_CUSTOM_SIGNAL 1 
#define IKBD_DRAGONNELS_MENU 1
#define IKBD_FROGGIES_MENU 2
#define IKBD_TB2_MENU 3
#endif

BYTE keyboard_buffer_read();

/*
#if (defined(SSE_ACIA)) 
//temp moved from emulator.h to have it compile
#include "acia.decla.h"
EXT ACIA_STRUCT acia[2];
#endif
*/

#if defined(SSE_ACIA_IRQ_DELAY)
// not defined anymore (v3.5.2), see MFP
inline void PrepareEventCheckForAciaIkbdIn() {
  if(acia[0].rx_stage)
  {
    int tt=ikbd.timer_when_keyboard_info+HD6301_TO_ACIA_IN_CYCLES;
    if(acia[0].rx_stage==2)
    {
      tt+=SSE_ACIA_IRQ_DELAY_CYCLES; // fixes V8 Music Studio (from Hatari)
    }
    if((time_of_next_event-tt) >= 0)
    {
      time_of_next_event=tt;  
      screen_event_vector=event_acia_rx_irq;
    }
  }
}
#define PREPARE_EVENT_CHECK_FOR_ACIA_IKBD_IN PrepareEventCheckForAciaIkbdIn(); 
#endif


//#endif

#undef EXT
#undef INIT


#endif//IKBD_DECLA_H