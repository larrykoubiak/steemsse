#pragma once
#ifndef SSEDMA_H
#define SSEDMA_H

/*
Custom Atari DMA chip.
*/

#if defined(SS_DMA)
//TODO apart files; works for RW track
/*  We use object TDma to refine emulation of the DMA in Steem.
    Steem original variables are defined as the new ones.
*/

#define dma_sector_count Dma.Counter
#define dma_address Dma.BaseAddress
#define dma_bytes_written_for_sector_count Dma.ByteCount
#define dma_mode Dma.MCR
#define dma_status Dma.SR

struct TDma {

  MEM_ADDRESS BaseAddress; // Base Address Register
  WORD MCR; // mode control register
  BYTE SR; // status register
  WORD Counter; // Counter register
  WORD ByteCount; // 1-512 for sectors

  TDma();
  BYTE IORead(MEM_ADDRESS addr);
  void IOWrite(MEM_ADDRESS addr,BYTE io_src_b);
/*  Because Steem runs CAPS and Pasti plugins, DMA/FDC regs are not
    always up-to-date. This function copies the values from CAPS or
    Pasti so that they are. Useful for debug, OSD.
*/
  void UpdateRegs(bool trace_them=false);

#if defined(SS_STRUCTURE_DMA_INC_ADDRESS)
  void IncAddress();
#define DMA_INC_ADDRESS Dma.IncAddress();
#endif

#if defined(SS_DMA_FIFO)

  void AddToFifo(BYTE data);

#if defined(SS_DMA_DRQ)
  void Drq(); // void?
#endif

  BYTE GetFifoByte();

/*  To be able to emulate the DMA precisely if necessary, DMA transfers are
    emulated in two steps. First, they're requested, then they're executed.
    That way we can emulate a little delay before the transfer occurs & the
    toggling of two FIFO buffers at each transfer.
    It proved overkill however, so calling RequestTransfer() will result in
    the immediate call of TransferBytes() (as #defined in SSE.h).
    And only one buffer is used.
*/
  void RequestTransfer();
  void TransferBytes();
#if SSE_VERSION>=370
  unsigned int Request:1;//even so it works, no need for a nested struct
  unsigned int BufferInUse:1;
  unsigned int Fifo_idx:5; //5 bits, must reach 16!
#else
  bool Request;
  BYTE Fifo_idx;
#if defined(SS_DMA_DOUBLE_FIFO)
  bool BufferInUse; 
#endif
#endif
#if defined(SS_DMA_DOUBLE_FIFO)
/*
"Internally the DMA has two 16 bytes FIFOs that are used alternatively. 
This feature allows the DMA to continue to receive bytes from the FDC/HDC
 controller while waiting for the processor to read the other FIFO. 
When a FIFO is full a bus request is made to the 68000 and when granted,
 the FIFO is transferred to the memory. This continues until all bytes 
have been transferred."
This feature is emulated in Steem if SS_DMA_DOUBLE_FIFO is defined, but it
hasn't proved necessary yet.
TODO: maybe use the feature and remove #define to make code more readable?
*/
#if SSE_VERSION<370
//  bool BufferInUse; 
#endif
  BYTE Fifo[2][16
#if defined(SS_DMA_FIFO_READ_ADDRESS) && !defined(SS_DMA_FIFO_READ_ADDRESS2)
    +4 // see fdc.h, replaces fdc_read_address_buffer[20]//review this!
#endif
    ];
#else
  BYTE Fifo[16
#if defined(SS_DMA_FIFO_READ_ADDRESS) && !defined(SS_DMA_FIFO_READ_ADDRESS2)
    +4 // see fdc.h, replaces fdc_read_address_buffer[20]
#endif
    ];
#endif
#endif//FIFO
#if defined(SS_DMA_DELAY)
/*
"In read mode, when one of the FIFO is full (i.e. when 16 bytes have been
 transferred from the FDC or HDC) the DMA chip performs a bus request to
 the 68000 and in return the processor will grant the control of the bus to
  the DMA, the transfer to the memory is then done with 8 cycles then the 
bus is released to the system. As the processor takes time to grant 
the bus and as transferring data from FIFO to memory also takes time,
 the other FIFO is used for continuing the data transfer with the FD/HD
 controllers."
When SS_DMA_DELAY is defined (normally not), an arbitrary delay 
(SSEParameters.h) is applied before the transfer takes place. 
SS_DMA_DOUBLE_FIFO should also be defined (or the single buffer would be
overwritten).
This feature uses Steem's event system, which is cycle accurate but also 
heavy. It was a test.


  // if we use event for all, there's no need for this one (which delay
  we're not sure we need to emulate
*/
  static void Event(); 
  int TransferTime;
#endif

#if defined(SS_DMA_TRACK_TRANSFER)
  WORD Datachunk; // to check # 16byte parts
#endif

};

extern TDma Dma;

#endif//dma



#endif//SSEDMA_H
