/* <<<                                  */
/* Copyright (c) 1994-1996 Arne Riiber. */
/* All rights reserved.                 */
/* >>>                                  */
/*
 *  Timer functions
 */
#include "defs.h"
#include "chip.h"           /* chip address definitions */
#include "ireg.h"

#ifdef USE_PROTOTYPES
#include "memory.h"
#include "timer.h"
#endif

 
static int tcsr_is_read = 0;

//TODO? it's possible to write on FRC, see Hitachi doc

tcsr_getb (offs)
  u_int  offs;
{
  int tcsr;

  if ((tcsr = ireg_getb (TCSR)) & OCF)
    tcsr_is_read = 1;

#if defined(SSE_IKBD_6301_393_REF)
  // clear TOF but we don't check if SR was read 
  ireg_putb (TCSR, ireg_getb (TCSR) & (~TOF)); 
#endif

  return tcsr;
}

tcsr_putb (offs, value)
  u_int  offs;
  u_char value;
{
  u_char read_only = (ICF|OCF|TOF);

  ireg_putb (TCSR, ireg_getb (TCSR) & read_only | value & ~read_only);
}

/*
 * ocr_putb - write to high/low byte of OCR
 *
 * 6801 Output Compare Flag is cleared if TSR is read and
 * Output Compare Register hi or low byte is written
 */
ocr_putb (offs, value)
  u_int  offs;
  u_char value;
{

  ireg_putb (offs, value);
  /*
   * Clear OCF if TCSR is read
   */
  if (tcsr_is_read) {
    ireg_putb (TCSR, ireg_getb (TCSR) & ~OCF);
    tcsr_is_read = 0;
  }
}

/*
 * timer_inc - increment timer, given the number of clock cycles passed
 *
 * 6801 has prescaler of 1
 */
timer_inc (ncycles)
  u_int ncycles;
{
  u_int  frc_old;     /* Free Running Counter */
#if defined(SSE_IKBD_6301_393_REF)
  u_short frc_new; // 16 bit, must overflow
#else
  u_long frc_new;     /* 32 bits */
#endif
  u_int  ocr;     /* Output Compare Register */

  /*
   * Get old and compute new timer value
   */
  frc_old = ireg_getw (FRC);
  frc_new = frc_old + ncycles;

  /*
   * Check against timer compare registers
   */
  ocr = ireg_getw (OCR);

#if defined(SSE_IKBD_6301_393_REF)
/*  Argh! HD6301_TIMER_FIX below wasn't defined (name change) so our fix wasn't 
    compiled. Timings were messed up with some random effects (Warp STX, 
    Defulloir...).
*/
  // detect overflow (AFAIK nothing uses it)
  if(frc_old>frc_new)
    ireg_putb (TCSR, ireg_getb (TCSR) | TOF);

  //  detect output compare 
  if(frc_new>=ocr && (frc_old<ocr || frc_old>frc_new)) {
#else
  if (frc_new >= ocr
#if defined(HD6301_TIMER_FIX)
/*  According to Hitachi doc, the OCF flag is set when the match is exact, 
    the comparison is constant. In the emu, cycles are added according to the
    instruction timings, hence the >=. But once the flag has been set, it
    shouldn't be set again if the OCR value isn't changed.
    The max timing seems to be 12 according to optab.c.
    Not sure it's a real fix, could even be a bug!
*/    
    && frc_new-ocr<14 
#endif
    ) {
#endif
    ireg_putb (TCSR, ireg_getb (TCSR) | OCF);
    tcsr_is_read = 0;
  }
  ireg_putw (FRC, frc_new);
}
