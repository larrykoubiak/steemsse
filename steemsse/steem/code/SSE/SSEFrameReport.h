#pragma once
#ifndef SSEFRAMEREPORT_H
#define SSEFRAMEREPORT_H

/*  This is a copy of SSEShifterEvents with 'Frame' instead of
    'Shifter' because a frame report may contain other
    interesting events. Later SSEShifterEvents will be 
    removed.
*/
#if defined(SS_DEBUG_FRAME_REPORT)

#if defined(SS_STRUCTURE_SSEFRAMEREPORT_OBJ)
#include "SSEDebug.h"
#endif

#if defined(SS_DEBUG_FRAME_REPORT_MASK)
/*  We use the new general fake io control masks, they are 16bit, but 
    only 8 bits should be used for the GUI.
    Higher bits also for the GUI.
*/

//mask 1
#define FRAME_REPORT_MASK1 (Debug.ControlMask[0])
#define FRAME_REPORT_MASK_SYNCMODE                 (1<<15)
#define FRAME_REPORT_MASK_SHIFTMODE                (1<<14)
#define FRAME_REPORT_MASK_PAL                      (1<<13)
#define FRAME_REPORT_MASK_SDP_READ                 (1<<12)
#define FRAME_REPORT_MASK_SDP_WRITE                (1<<11)
#define FRAME_REPORT_MASK_SDP_LINES                (1<<10)
#define FRAME_REPORT_MASK_HSCROLL                  (1<<9)
#define FRAME_REPORT_MASK_VIDEOBASE                (1<<8)

//mask 2
#define FRAME_REPORT_MASK2 (Debug.ControlMask[1])
#define FRAME_REPORT_MASK_ACIA                     (1<<15)
#define FRAME_REPORT_MASK_BLITTER                  (1<<14)
#define FRAME_REPORT_MASK_SHIFTER_TRICKS           (1<<13)
#define FRAME_REPORT_MASK_SHIFTER_TRICKS_BYTES     (1<<12)
//possible to make just one:
#define FRAME_REPORT_MASK_VBI                      (1<<11)
#define FRAME_REPORT_MASK_HBI                      (1<<10)
#define FRAME_REPORT_MASK_MFP                      (1<<9)


#endif

////#define FRAME_REPORT_MASK_VERTICAL_OVERSCAN        (1<<13)    // trace!!!, not here

struct SFrameEvent {
  int Scanline; 
  int Cycle;
  char Type;
  int Value; 
  inline void Add(int scanline,int cycle, char type, int value); 
};


class TFrameEvents {
public:
  enum {MAX_EVENTS=210*32*2*2}; // too high?
  int TriggerReport; // set 2 to ask a full report, then it's set FALSE again
  TFrameEvents();
  inline void Add(int scanline, int cycle, char type, int value);
  void Init();
  int Report();
  void ReportLine();
  int Vbl(); 
  int nVbl;
  struct SFrameEvent m_FrameEvent[MAX_EVENTS]; // it's public
#if defined(SS_DEBUG_REPORT_SDP_ON_CLICK)
  MEM_ADDRESS GetSDP(int guessed_x,int guessed_scan_y);
#endif
private:
  int m_nEvents; // how many video events occurred this vbl?
  int m_nReports;
};


extern TFrameEvents FrameEvents; // singleton

#if defined(SS_DEBUG)
#define REPORT_LINE FrameEvents.ReportLine()
#else
#define REPORT_LINE
#endif

// inline functions


inline void SFrameEvent::Add(int scanline,int cycle, char type, int value) {
  Scanline=scanline;
  Cycle=cycle;
  Type=type;
  Value=value;
}



extern int shifter_freq_at_start_of_vbl; // forward


inline void TFrameEvents::Add(int scanline, int cycle, char type, int value) {
  m_nEvents++;  // starting from 0 each VBL, event 0 is dummy 
  if(m_nEvents<=0||m_nEvents>MAX_EVENTS) {BRK(bad m_nEvents); return;}
  int total_cycles= (shifter_freq_at_start_of_vbl==50) ?512:508;// Shifter.CurrentScanline.Cycles;//512;
  m_FrameEvent[m_nEvents].Add(scanline, cycle, type, value);
}

#endif

#endif//#ifndef SSEFRAMEREPORT_H