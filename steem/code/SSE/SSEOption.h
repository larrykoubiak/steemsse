#pragma once
#ifndef SSEOPTION_H
#define SSEOPTION_H
/*  SSEOption defines structures TOption (player choices) and TConfig 
    (internal use).
*/

#include "SSE.h" 

#ifdef WIN32
#include <windows.h>
#endif

#include "SSEDecla.h" //BYTE


struct TOption {

  BYTE STModel;
  BYTE DisplaySize;
  BYTE WakeUpState; 
  // More than 32 fields OK, if not we would get a warning (I hope)
  unsigned int Hacks:1;
  unsigned int Chipset1:1;
  unsigned int Microwire:1;
  unsigned int PSGFilter:1;
  unsigned int CaptureMouse:1;
  unsigned int StealthMode:1;
  unsigned int OutputTraceToFile:1; 
  unsigned int TraceFileLimit:1; // stop TRACING to file at +-3+MB 
  unsigned int UseSDL:1;
  unsigned int OsdDriveInfo:1;
  unsigned int Dsp:1; // giving the ability to disable buggy DSP
  unsigned int OsdImageName:1;
  unsigned int PastiJustSTX:1;
  unsigned int Interpolate:1;
  unsigned int StatusBar:1;
  unsigned int WinVSync:1;
  unsigned int TripleBuffer:1;
  unsigned int StatusBarGameName:1;
  unsigned int DriveSound:1;
  unsigned int SingleSideDriveMap:2;
  unsigned int PSGMod:1;
  unsigned int PSGFixedVolume:1;
  unsigned int GhostDisk:1;
  unsigned int Direct3D:1;
  unsigned int STAspectRatio:1;
  unsigned int DriveSoundSeekSample:1;
  unsigned int TestingNewFeatures:1;
  unsigned int BlockResize:1;
  unsigned int LockAspectRatio:1;
  unsigned int FinetuneCPUclock:1;
  unsigned int Chipset2:1;
  unsigned int PRG_support:1;
  unsigned int Direct3DCrisp:1;
  unsigned int Acsi:1;
  unsigned int KeyboardClick:1;
  unsigned int MonochromeDisableBorder:1;
  unsigned int FullScreenGui:1;
  unsigned int VMMouse:1;
  unsigned int OsdTime:1;

#ifdef __cplusplus // visible only to C++ objects
  TOption();
  void Init();
#endif
};

// C linkage to be accessible by 6301 emu
#ifdef __cplusplus
extern "C" TOption SSEOption;
#else
extern struct TOption SSEOption;
#endif

// these macros were considered useful at some point
#define OPTION_HACKS (SSEOption.Hacks)
#define OPTION_C1 (SSEOption.Chipset1)
#define MICROWIRE_ON (SSEOption.Microwire)
#define PSG_FILTER_FIX (SSEOption.PSGFilter)
#define ST_TYPE (SSEOption.STModel)
#define OPTION_CAPTURE_MOUSE (SSEOption.CaptureMouse)
#if !defined(SSE_VID_BORDERS_LB_DX1) // see SSEDecla.h
#define BORDER_40 (SSEOption.DisplaySize==1)
#endif
#define DISPLAY_SIZE (SSEOption.DisplaySize)
#define STEALTH_MODE SSEOption.StealthMode
#define USE_TRACE_FILE (SSEOption.OutputTraceToFile)
#define TRACE_FILE_REWIND (SSEOption.TraceFileLimit)
#define OPTION_WS (SSEOption.WakeUpState)
#define USE_SDL (SSEOption.UseSDL)
#define OSD_DRIVE_INFO (SSEOption.OsdDriveInfo)
#define DSP_ENABLED (SSEOption.Dsp)
#define OSD_IMAGE_NAME (SSEOption.OsdImageName)
#define PASTI_JUST_STX (SSEOption.PastiJustSTX)
#define SSE_INTERPOLATE (SSEOption.Interpolate)
#define SSE_STATUS_BAR (SSEOption.StatusBar)
#define SSE_STATUS_BAR_GAME_NAME (SSEOption.StatusBarGameName)
#define SSE_WIN_VSYNC (SSEOption.WinVSync)
#define SSE_3BUFFER (SSEOption.TripleBuffer)
//#define SSE_DRIVE_SOUND (SSEOption.DriveSound)
#define SSE_GHOST_DISK (SSEOption.GhostDisk)
#define SSE_OPTION_D3D (SSEOption.Direct3D)
#define SSE_OPTION_PSG (SSEOption.PSGMod)
#define SSE_OPTION_PSG_FIXED (SSEOption.PSGFixedVolume)
#define OPTION_ST_ASPECT_RATIO (SSEOption.STAspectRatio)
#define DRIVE_SOUND_SEEK_SAMPLE (SSEOption.DriveSoundSeekSample)
#define SSE_TEST_ON (SSEOption.TestingNewFeatures)//use macro only for actual tests
#define OPTION_BLOCK_RESIZE (SSEOption.BlockResize)
#define OPTION_LOCK_ASPECT_RATIO (SSEOption.LockAspectRatio)
#define OPTION_CPU_CLOCK (SSEOption.FinetuneCPUclock)
#define OPTION_C2 (SSEOption.Chipset2)
#define OPTION_PRG_SUPPORT (SSEOption.PRG_support)
#define OPTION_D3D_CRISP (SSEOption.Direct3DCrisp)
#define OPTION_KEYBOARD_CLICK (SSEOption.KeyboardClick)
#define OPTION_FULLSCREEN_GUI (SSEOption.FullScreenGui)
#define OPTION_OSD_TIME (SSEOption.OsdTime)

#if defined(SSE_SSE_CONFIG_STRUCT)

struct TConfig {

  int FullscreenMask; // mask?
  
  unsigned int UnrarDll:1;
  //unsigned int SdlDll:1;//forget it?
  unsigned int Hd6301v1Img:1;
  unsigned int unzipd32Dll:1;
  unsigned int CapsImgDll:1;
  unsigned int PastiDll:1;
  unsigned int Direct3d9:1;
  unsigned int ArchiveAccess:1;
  unsigned int AcsiImg:1;
  //unsigned int Stemdos:1; //unused
  unsigned int VideoCard8bit:1;
  unsigned int VideoCard16bit:1;
  unsigned int ym2149_fixed_vol:1;
  

#ifdef __cplusplus // visible only to C++ objects
  TConfig();
#endif
};

// C linkage to be accessible by 6301 emu (and maybe others)
#ifdef __cplusplus
extern "C" TConfig SSEConfig;
#else
extern struct TConfig SSEConfig;
#endif

#define CAPSIMG_OK (SSEConfig.CapsImgDll)
#define DX_FULLSCREEN (SSEConfig.FullscreenMask)
#define HD6301_OK (SSEConfig.Hd6301v1Img)
#define SDL_OK (SSEConfig.SdlDll)
#define UNRAR_OK (SSEConfig.UnrarDll)
#define D3D9_OK (SSEConfig.Direct3d9)
#define ARCHIVEACCESS_OK (SSEConfig.ArchiveAccess)

#if defined(SSE_ACSI_HDMAN)
#define ACSI_EMU_ON (SSEConfig.AcsiImg && SSEOption.Acsi)
#elif defined(SSE_ACSI)
#define ACSI_EMU_ON (!HardDiskMan.DisableHardDrives && SSEConfig.AcsiImg && SSEOption.Acsi)
#else
#define ACSI_EMU_ON 0
#endif

#else//#if ! defined(SSE_SSE_CONFIG_STRUCT)

#define CAPSIMG_OK (Caps.Version)
#define DD_FULLSCREEN (iDummy)
#define HD6301_OK (HD6301.Initialised)
#define SDL_OK (iDummy)
#define UNRAR_OK (iDummy)
#define D3D9_OK (iDummy)

#endif//#if defined(SSE_SSE_CONFIG_STRUCT)

#endif//#ifndef SSEOPTION_H

