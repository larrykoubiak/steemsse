/*---------------------------------------------------------------------------
FILE: draw_c_medres_scanline.cpp
MODULE: draw_c
DESCRIPTION: Med res C++ drawing routine
---------------------------------------------------------------------------*/

//void draw_scanline_medres_pixelwise_8(int border1,int picture,int border2,int hscroll){
  int n;
  WORD w0,w1,mask;
  MEM_ADDRESS source;
  picture*=2;
  GET_START(0,160)
#if defined(SSE_DRAW_C_383B)
  DRAW_BORDER_PIXELS(border1)
#else
  n=border1/16;DRAW_BORDER(n);
  for(border1&=15;border1>0;border1--){
    DRAWPIXEL(PCpal);
  }
#endif
  if(picture){
    n=16-hscroll;
    if(picture<n)n=picture;
    if(n<16){ //draw a bit of a raster
      picture-=n;
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
      mask=WORD(0x8000 >> hscroll);
      for(;n>0;n--){
        CALC_COL_MEDRES_AND_DRAWPIXEL(mask);
        mask>>=1;
      }
    }
    for(n=picture/16;n>0;n--){
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
#if defined(SSE_DRAW_C_383A) //anyway, to reduce bloat
      for(int mask=BIT_15;mask;mask>>=1)
      {
        CALC_COL_MEDRES_AND_DRAWPIXEL(mask);
      }
#else
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_15);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_14);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_13);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_12);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_11);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_10);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_9);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_8);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_7);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_6);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_5);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_4);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_3);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_2);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_1);
      CALC_COL_MEDRES_AND_DRAWPIXEL(BIT_0);
#endif
    }
    picture&=15;
    if(picture){
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
      mask=0x8000;
      for(;picture>0;picture--){
        CALC_COL_MEDRES_AND_DRAWPIXEL(mask);
        mask>>=1;
      }
    }
  }
#if defined(SSE_DRAW_C_383B)
  DRAW_BORDER_PIXELS(border2)
#else
  n=border2/16;DRAW_BORDER(n);
  for(border2&=15;border2>0;border2--){
    DRAWPIXEL(PCpal);
  }
#endif
