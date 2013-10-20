#pragma once
#ifndef MACROS_DECLA_H
#define MACROS_DECLA_H


#define EXT extern
#define INIT(s)

#define MACRO_DEFAULT_ADD_MOUSE 1
#define MACRO_DEFAULT_ALLOW_VBLS 1
#define MACRO_DEFAULT_MAX_MOUSE 15

EXT void macro_advance(int DEFVAL(0));
EXT void macro_end(int);

EXT void macro_record_joy();
EXT void macro_record_mouse(int,int);
EXT void macro_record_key(BYTE);

EXT bool macro_play_start();
EXT void macro_play_joy();
EXT void macro_play_mouse(int&,int&);
EXT void macro_play_keys();

EXT int macro_record INIT(0),macro_play INIT(0),macro_play_until;
EXT bool macro_play_has_mouse INIT(0),macro_play_has_keys INIT(0),macro_play_has_joys INIT(0);
EXT int macro_start_after_ikbd_read_count INIT(0);
EXT int macro_play_max_mouse_speed INIT(MACRO_DEFAULT_MAX_MOUSE);

EXT DWORD macro_jagpad[2];

//#ifdef IN_MAIN
#define MACRO_LEFT_INIT_MOVE (shifter_x*2)
#define MACRO_UP_INIT_MOVE (shifter_y*2)
/*
#define MACRO_LEFT_INIT_MOVE (shifter_x)
#define MACRO_UP_INIT_MOVE (shifter_y)
*/
#define MACRO_STARTRECORD 1
#define MACRO_STARTPLAY 2

#define MACRO_ENDRECORD 1
#define MACRO_ENDPLAY 2

#define MACRO_RECORD_BUF_INC_SECS 20

typedef struct{
  int xdiff,ydiff;
  BYTE stick[8],keycode[32];
  DWORD keys;
  DWORD jagpad[2];
}MACROVBLINFO;

typedef struct{
  int add_mouse_together,allow_same_vbls,max_mouse_speed;
}MACROFILEOPTIONS;

#define MACRO_FILE_GET 0
#define MACRO_FILE_SET 1

bool macro_mvi_blank(MACROVBLINFO *);
void macro_file_options(int,char*,MACROFILEOPTIONS *,FILE* = NULL);

EXT Str macro_play_file,macro_record_file;

EXT DynamicArray<MACROVBLINFO> macro_record_store,macro_play_store;
EXT MACROVBLINFO *mrsc,*mpsc;
//#endif//#ifdef IN_MAIN

#undef EXT
#undef INIT

#endif//#ifndef MACROS_DECLA_H