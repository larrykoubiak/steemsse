/* <<<                                  */
/* Copyright (c) 1994-1996 Arne Riiber. */
/* All rights reserved.                 */
/* >>>                                  */
/*
 *  General purpose CPU info
 */
#include "defs.h"
#include "cpu.h"
#include "reg.h"

struct cpu cpu;

cpu_reset ()
{
  reset ();   /* chip specific reset */
  cpu_setstackmin (cpu_getstackmin ());
  cpu_setncycles (0);
  /*
   * The following suits many CPU's but could also test CPU type
   */
#if !defined(SS_IKBD_6301_DISABLE_CALLSTACK)
  cpu_setstackmax (cpu_getstackmax () ? cpu_getstackmax () : 0x00FF);
#else
  cpu_setstackmax (0x00FF);
#endif
}

cpu_print ()
{
  reg_printall ();
  printf ("\t[%d]\n", cpu_getncycles ());
  instr_print (reg_getpc ());
}
