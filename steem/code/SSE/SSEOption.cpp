#include "SSE.h"
#if defined(SSE_BUILD)
#include "../pch.h"
#include <emulator.decla.h>
#include "SSEOption.h"
#include "SSEDebug.h"
#include "SSEInterrupt.h"
#include "SSEVideo.h"

TOption SSEOption; // singleton

//tmp
#ifdef UNIX
#include <stdio.h>
#include <string.h>
#endif

TOption::TOption() {
  Init(); // a problem...
}


void TOption::Init() {

#ifdef WIN32
  ZeroMemory(this,sizeof(TOption));
#else
  memset(this,0,sizeof(TOption));
#endif

#if defined(SSE_HACKS)
  Hacks=true;
#endif
#if defined(SSE_IKBD_6301)
  Chipset1=true;
#endif
  Chipset2=true;
#if defined(DEBUG_BUILD)  
  OutputTraceToFile=1;
#endif
  OsdDriveInfo=1;
  StatusBar=1;
  DriveSound=1;
#ifdef SSE_VID_D3D
  Direct3D=1;
#endif
  SampledYM=true;
//393
  Microwire=1;
  EmuDetect=1;
  PastiJustSTX=1;
  FakeFullScreen=1; // safer than 32x200!
  KeyboardClick=1;
}

TConfig SSEConfig;

#if defined(SSE_GUI_FONT_FIX)
HFONT my_gui_font=NULL; // use global, not member, because of constructor order
#endif

TConfig::TConfig() {
#ifdef WIN32
  ZeroMemory(this,sizeof(TConfig));
#else
  memset(this,0,sizeof(TConfig));
#endif
}

TConfig::~TConfig() {
#if defined(SSE_GUI_FONT_FIX)
  if(my_gui_font)
  {
    DeleteObject(my_gui_font); // free Windows resource
#ifndef LEAN_AND_MEAN
    my_gui_font=NULL;
#endif
  }
#endif
}

#if defined(SSE_GUI_FONT_FIX)
/*  Steem used DEFAULT_GUI_FONT at several places to get the GUI font.
    On some systems it is different, and it can seriously mess the GUI.
    With this mod, we create a logical font with the parameters of
    DEFAULT_GUI_FONT on a normal system.
    Only if it fails do we use DEFAULT_GUI_FONT.
    Notice that this function is called as soon as needed, which is
    in a constructor. That means we can't reliably use TRACE() here.
update: turns out it was a problem of Windows size setting instead, so
this isn't enabled after all
*/

HFONT TConfig::GuiFont() {
  if(my_gui_font==NULL)
  {
    my_gui_font=CreateFont(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE,
      "MS Shell Dlg");
    ASSERT(my_gui_font);
    if(!my_gui_font) // fall back
      my_gui_font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
#ifdef SSE_DEBUG__
    LOGFONT fi;
    GetObject(my_gui_font,sizeof(fi),&fi);
#endif
  }
  return my_gui_font;
}

#endif


#if defined(SSE_VID_BPP_CHOICE) && !defined(SSE_VID_BPP_NO_CHOICE) 

int TConfig::GetBitsPerPixel() {
  int bpp=(SSEConfig.VideoCard32bit)?32:16; // default
  if(display_option_fs_bpp==1)
  {
    if(SSEConfig.VideoCard8bit)
      bpp=16;
  }
  else if (display_option_fs_bpp==0)
  {
    if(SSEConfig.VideoCard8bit)
      bpp=8;
    else if(SSEConfig.VideoCard16bit)
      bpp=16;
  }
  return bpp;
}

#endif

#if defined(SSE_STF)

int TConfig::SwitchSTType(int new_type) { 

  ASSERT(new_type>=0 && new_type<N_ST_MODELS);
  ST_TYPE=new_type;
  if(ST_TYPE!=STE) // all STF types
  {
#if defined(SSE_CPU_MFP_RATIO)
#if defined(SSE_CPU_MFP_RATIO_OPTION)
    if(OPTION_CPU_CLOCK)
      CpuMfpRatio=(double)CpuCustomHz/(double)MFP_CLOCK;
    else
#endif
    {
#if defined(SSE_STF_MEGASTF_CLOCK)
      if(ST_TYPE==MEGASTF)
      {
        CpuMfpRatio=((double)CPU_STF_MEGA-0.5)/(double)MFP_CLOCK; //it's rounded up in hz
        CpuNormalHz=CPU_STF_MEGA;
      }
      else
      {
        CpuMfpRatio=(double)CPU_STF_PAL/(double)MFP_CLOCK;
        CpuNormalHz=CPU_STF_PAL;
      }
#else
      CpuMfpRatio=(double)CPU_STF_PAL/(double)MFP_CLOCK;
      CpuNormalHz=CPU_STF_PAL;
#endif
    }
#endif
  }
  else //STE
  {
#if defined(SSE_CPU_MFP_RATIO)
#if defined(SSE_CPU_MFP_RATIO_OPTION)
    if(OPTION_CPU_CLOCK)
      CpuMfpRatio=(double)CpuCustomHz/(double)MFP_CLOCK;
    else
#endif
    {
    CpuMfpRatio=(double)CPU_STE_PAL/(double)MFP_CLOCK;
    CpuNormalHz=CPU_STE_PAL; 
    }
#endif

  }
  
#if defined(SSE_CPU_MFP_RATIO)
  TRACE_INIT("CPU~%d hz\n",CpuNormalHz);
#if defined(SSE_CPU_MFP_RATIO_HIGH_SPEED) && !defined(SSE_GUI_NO_CPU_SPEED)
  if(n_cpu_cycles_per_second<10000000) // avoid interference with ST CPU Speed option
#endif
    n_cpu_cycles_per_second=CpuNormalHz; // no wrong CPU speed icon in OSD (3.5.1)
#endif
#if defined(SSE_GLUE)
  Glue.Update();
#endif

  return ST_TYPE;
}

#endif

#endif//sse