#pragma once
#ifndef SSEACSI_H
#define SSEACSI_H

#include <conditions.h>
#pragma pack(push, STRUCTURE_ALIGNMENT)

struct TAcsiHdc {
enum {MAX_ACSI_DEVICES=4}; // could be 8 but we stop at 4
  TAcsiHdc();
  ~TAcsiHdc();
  // interface
  bool Init(int num,char *path);
#if defined(SSE_VS2008_WARNING_382)
  BYTE IORead();
#else
  BYTE IORead(BYTE Line);
#endif
  void IOWrite(BYTE Line,BYTE io_src_b);
  void Irq(bool state);
#if defined(SSE_VS2008_WARNING_382)
  void Reset();
#else
  void Reset(bool Cold);
#endif
  // other functions
  void CloseImageFile();
  void ReadWrite(bool write,BYTE block_count);
  int SectorNum();
  bool Seek();
#if defined(SSE_ACSI_FORMAT)
  void Format();
#endif
#if defined(SSE_ACSI_INQUIRY) 
  void Inquiry();
#endif
  // member variables
  int nSectors; //total
#if defined(SSE_ACSI_TIMING)
  int time_of_irq;
#endif
#if defined(SSE_ACSI_INQUIRY2)
  char inquiry_string[32];
#endif
  FILE *hard_disk_image;
  BYTE device_num; //0-7
  BYTE cmd_block[6];
  BYTE cmd_ctr;
  BYTE STR,DR; //STR 0=OK
  BYTE error_code;
  BYTE Active; // can't be bool, can be 2
};

#pragma pack(pop)

extern BYTE acsi_dev;
extern TAcsiHdc AcsiHdc[TAcsiHdc::MAX_ACSI_DEVICES]; // each object is <64 bytes

#endif//#ifndef SSEACSI_H