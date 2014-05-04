#if defined(STEVEN_SEAGAL) && defined(SSE_STRUCTURE_PSG_H)

#include "psg.decla.h"

#else//!SSE_STRUCTURE_PSG_H


#ifdef IN_EMU
#define EXT
#define INIT(s) =s
#else
#define EXT extern
#define INIT(s)
#endif

/*  tone frequency
     fT = fMaster
          -------
           16TP


ie. period = 16TP / 2 000 000
= TP/125000

 = TP / (8*15625)


 the envelope repetition frequency fE is obtained as follows from the
envelope setting period value EP (decimal):

       fE = fMaster        (fMaster if the frequency of the master clock)
            -------
             256EP

The period of the actual frequency fEA used for the envelope generated is
1/32 of the envelope repetition period (1/fE).

ie. period of whole envelope = 256EP/2 000 000 = 2*EP / 15625 (think this is one go through, eg. /)
Period of envelope change frequency is 1/32 of this, so EP/ 16*15625
Scale by 64 and multiply by frequency of buffer to get final period, 4*EP/15625

*/

/*      New scalings - 6/7/01

__TONE__  Period = TP/(8*15625) Hz = (TP*sound_freq)/(8*15625) samples
          Period in samples goes up to 1640.6.  Want this to correspond to 2000M, so
          scale up by 2^20 = 1M.  So period to initialise counter is
          (TP*sound_freq*2^17)/15625, which ranges up to 1.7 thousand million.  The
          countdown each time is 2^20, but we double this to 2^21 so that we can
          spot each half of the period.  To initialise the countdown, bit 0 of
          (time*2^21)/tonemodulo gives high or low.  The counter is initialised
          to tonemodulo-(time*2^21 mod tonemodulo).

__NOISE__ fudged similarly to tone.

__ENV__   Step period = 2EP/(15625*32) Hz.  Scale by 2^17 to scale 13124.5 to 1.7 thousand
          million.  Step period is (EP*sound_freq*2^13)/15625.

*/




#define SOUND_DESIRED_LQ_FREQ (50066/2)

#ifdef ENABLE_VARIABLE_SOUND_DAMPING
EXT int sound_variable_a INIT(32);
EXT int sound_variable_d INIT(208);
#endif

#define SOUND_MODE_MUTE         0
#define SOUND_MODE_CHIP         1
#define SOUND_MODE_EMULATED     2
#define SOUND_MODE_SHARPSAMPLES 3
#define SOUND_MODE_SHARPCHIP    4

EXT bool sound_internal_speaker INIT(false);
EXT int sound_freq INIT(50066),sound_comline_freq INIT(0),sound_chosen_freq INIT(50066);
EXT int sound_mode INIT(SOUND_MODE_CHIP),sound_last_mode INIT(SOUND_MODE_CHIP);
EXT BYTE sound_num_channels INIT(1),sound_num_bits INIT(8);
EXT int sound_bytes_per_sample INIT(1);
EXT DWORD MaxVolume INIT(0xffff);
EXT bool sound_low_quality INIT(0);
EXT bool sound_write_primary INIT( NOT_ONEGAME(0) ONEGAME_ONLY(true) );
EXT bool sound_click_at_start INIT(0);
EXT int sound_time_method INIT(0);
EXT bool sound_record INIT(false);
EXT DWORD sound_record_start_time; //by timer variable = timeGetTime()
EXT int psg_write_n_screens_ahead INIT(3 UNIX_ONLY(+7) );

EXT void SoundStopInternalSpeaker();

EXT int psg_voltage,psg_dv;

#define PSGR_PORT_A 14
#define PSGR_PORT_B 15

EXT int psg_reg_select;
EXT BYTE psg_reg[16],psg_reg_data;

EXT FILE *wav_file INIT(NULL);

#if defined(ENABLE_LOGFILE) || defined(SHOW_WAVEFORM)
EXT DWORD min_write_time;
EXT DWORD play_cursor,write_cursor;
#endif

#define DEFAULT_SOUND_BUFFER_LENGTH (32768*SCREENS_PER_SOUND_VBL)
EXT int sound_buffer_length INIT(DEFAULT_SOUND_BUFFER_LENGTH);
EXT DWORD SoundBufStartTime;

#define PSG_BUF_LENGTH sound_buffer_length

#if SCREENS_PER_SOUND_VBL == 1
//#define MOD_PSG_BUF_LENGTH &(PSG_BUF_LENGTH-1)
#define MOD_PSG_BUF_LENGTH %PSG_BUF_LENGTH
#else
#define MOD_PSG_BUF_LENGTH %PSG_BUF_LENGTH
EXT int cpu_time_of_last_sound_vbl INIT(0);
#endif

EXT DWORD psg_last_play_cursor;
EXT DWORD psg_last_write_time;
EXT DWORD psg_time_of_start_of_buffer;
EXT DWORD psg_time_of_last_vbl_for_writing,psg_time_of_next_vbl_for_writing;
EXT int psg_n_samples_this_vbl;

#ifdef SHOW_WAVEFORM

EXT int temp_waveform_display_counter;
#define MAX_temp_waveform_display_counter PSG_BUF_LENGTH
EXT BYTE temp_waveform_display[DEFAULT_SOUND_BUFFER_LENGTH];
EXT void draw_waveform();
EXT DWORD temp_waveform_play_counter;
//#define TEMP_WAVEFORM_INTERVAL 31

#endif

EXT void dma_sound_get_last_sample(WORD*,WORD*);

EXT void psg_capture(bool,Str),psg_capture_check_boundary();
EXT FILE *psg_capture_file INIT(NULL);
EXT int psg_capture_cycle_base INIT(0);

EXT bool psg_always_capture_on_start INIT(0);


#ifdef IN_EMU

#define ONE_MILLION 1048576
#define TWO_MILLION 2097152
//two to the power of 21
#define TWO_TO_SIXTEEN 65536
#define TWO_TO_SEVENTEEN 131072
#define TWO_TO_EIGHTEEN 262144

bool sound_first_vbl=0;

void sound_record_to_wav(int,DWORD,bool,int*);
//---------------------------------------------------------------------------
HRESULT Sound_VBL();
//--------------------------------------------------------------------------- DMA Sound
void dma_sound_fetch();
void dma_sound_set_control(BYTE);
void dma_sound_set_mode(BYTE);

BYTE dma_sound_control,dma_sound_mode;
MEM_ADDRESS dma_sound_start=0,next_dma_sound_start=0,
            dma_sound_end=0,next_dma_sound_end=0;

WORD MicroWire_Mask=0x07ff;
WORD MicroWire_Data=0;
int MicroWire_StartTime=0;
#define CPU_CYCLES_PER_MW_SHIFT 8

int dma_sound_mode_to_freq[4]={6258,12517,25033,50066};
int dma_sound_freq,dma_sound_output_countdown,dma_sound_samples_countdown;

WORD dma_sound_internal_buf[4],dma_sound_last_word;
int dma_sound_internal_buf_len=0;
MEM_ADDRESS dma_sound_fetch_address;

// Max frequency/lowest refresh *2 for stereo
#define DMA_SOUND_BUFFER_LENGTH 2600 * SCREENS_PER_SOUND_VBL * 2

WORD dma_sound_channel_buf[DMA_SOUND_BUFFER_LENGTH+16];
DWORD dma_sound_channel_buf_last_write_t;
int dma_sound_on_this_screen=0;

#define DMA_SOUND_CHECK_TIMER_A \
    if (mfp_reg[MFPR_TACR]==8){ \
      mfp_timer_counter[0]-=64;  \
      if (mfp_timer_counter[0]<64){ \
        mfp_timer_counter[0]=BYTE_00_TO_256(mfp_reg[MFPR_TADR])*64; \
        mfp_interrupt_pend(MFP_INT_TIMER_A,ABSOLUTE_CPU_TIME); \
      }                                 \
    }

int dma_sound_mixer=1,dma_sound_volume=40;
int dma_sound_l_volume=20,dma_sound_r_volume=20;
int dma_sound_l_top_val=128,dma_sound_r_top_val=128;

#if defined(STEVEN_SEAGAL) && defined(SSE_SOUND_MICROWIRE)
#include "../../3rdparty/dsp/dsp.h"
int dma_sound_bass=6; // 6 is neutral value
int dma_sound_treble=6;
TIirVolume MicrowireVolume[2];
TIirLowShelf MicrowireBass[2];
TIirHighShelf MicrowireTreble[2];
#if defined(SSE_SOUND_VOL)
TIirVolume PsgGain;
#endif
#if defined(SSE_SOUND_LOW_PASS_FILTER)
TIirLowPass PSGLowFilter[2];
#endif
#endif//microwire



//---------------------------------- PSG ------------------------------------

void psg_write_buffer(int,DWORD);

//make constant for dimming waveform display

//#define PSG_BUF_LENGTH (32768*SCREENS_PER_SOUND_VBL)

#define PSG_NOISE_ARRAY 8192
#define MOD_PSG_NOISE_ARRAY & 8191

#ifndef ONEGAME
#define PSG_WRITE_EXTRA 300
#else
#if defined(STEVEN_SEAGAL) && defined(SSE_SOUND_NO_EXTRA_PER_VBL)
#define PSG_WRITE_EXTRA 0
#else
#define PSG_WRITE_EXTRA OGExtraSamplesPerVBL
#endif
#endif

//#define PSG_WRITE_EXTRA 10

//#define PSG_CHANNEL_BUF_LENGTH (2048*SCREENS_PER_SOUND_VBL)
#define PSG_CHANNEL_BUF_LENGTH (8192*SCREENS_PER_SOUND_VBL)
#define VOLTAGE_ZERO_LEVEL 0
#define VOLTAGE_FIXED_POINT 256
//must now be fixed at 256!
#define VOLTAGE_FP(x) ((x) << 8)

//BYTE psg_channel_buf[3][PSG_CHANNEL_BUF_LENGTH];
int psg_channels_buf[PSG_CHANNEL_BUF_LENGTH+16];
int psg_buf_pointer[3];
DWORD psg_tone_start_time[3];


//DWORD psg_tonemodulo_2,psg_noisemodulo;
//const short volscale[16]=  {0,1,1,2,3,4,5,7,12,20,28,44,70,110,165,255};

char psg_noise[PSG_NOISE_ARRAY];



#define PSGR_NOISE_PERIOD 6
#define PSGR_MIXER 7
#define PSGR_AMPLITUDE_A 8
#define PSGR_AMPLITUDE_B 9
#define PSGR_AMPLITUDE_C 10
#define PSGR_ENVELOPE_PERIOD 11
#define PSGR_ENVELOPE_PERIOD_LOW 11
#define PSGR_ENVELOPE_PERIOD_HIGH 12
#define PSGR_ENVELOPE_SHAPE 13

/*
       |     |13 Envelope Shape                         BIT 3 2 1 0|
       |     |  Continue -----------------------------------' | | ||
       |     |  Attack ---------------------------------------' | ||
       |     |  Alternate --------------------------------------' ||
       |     |  Hold ---------------------------------------------'|
       |     |   00xx - \____________________________________      |
       |     |   01xx - /|___________________________________      |
       |     |   1000 - \|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\      |
       |     |   1001 - \____________________________________      |
       |     |   1010 - \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\      |
       |     |   1011 - \|-----------------------------------      |
       |     |   1100 - /|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/      |
       |     |   1101 - /------------------------------------      |
       |     |   1110 - /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/      |
       |     |   1111 - /|___________________________________      |

*/

#define PSG_ENV_SHAPE_HOLD BIT_0
#define PSG_ENV_SHAPE_ALT BIT_1
#define PSG_ENV_SHAPE_ATTACK BIT_2
#define PSG_ENV_SHAPE_CONT BIT_3

#define PSG_CHANNEL_AMPLITUDE 60

//#define PSG_VOLSCALE(vl) (volscale[vl]/4+VOLTAGE_ZERO_LEVEL)
/*
#define PSG_ENV_DOWN 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
#define PSG_ENV_UP   00,01,02,03,04,05,6,7,8,9,10,11,12,13,14,15
#define PSG_ENV_0    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
#define PSG_ENV_LOUD 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15

const BYTE psg_envelopes[8][32]={
    {PSG_ENV_DOWN,PSG_ENV_DOWN},
    {PSG_ENV_DOWN,PSG_ENV_0},
    {PSG_ENV_DOWN,PSG_ENV_UP},
    {PSG_ENV_DOWN,PSG_ENV_LOUD},
    {PSG_ENV_UP,PSG_ENV_UP},
    {PSG_ENV_UP,PSG_ENV_LOUD},
    {PSG_ENV_UP,PSG_ENV_DOWN},
    {PSG_ENV_UP,PSG_ENV_0}};

*/
#define VFP VOLTAGE_FIXED_POINT
#define VZL VOLTAGE_ZERO_LEVEL
#define VA VFP*PSG_CHANNEL_AMPLITUDE
//const int psg_flat_volume_level[16]={0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP};
const int psg_flat_volume_level[16]={0*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
                                      17*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,48*VA/1000+VZL*VFP,
                                      69*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,
                                      287*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP};

/*
0 0     0      0
1 0.001 0.0045 0.0041
2 0.002 0.0081 0.0053
3 0.005 0.0117 0.008
4 0.02  0.0175 0.0124
5 0.05  0.0241 0.0158
6 0.065 0.0355 0.0211
7 0.08  0.048  0.0317
8 0.1   0.069  0.0596
9 0.125 0.095  0.0938
A 0.175 0.139  0.131
B 0.25  0.191  0.207
C 0.36  0.287  0.312
D 0.51  0.407  0.46
E 0.71  0.648  0.67
F 1     1      1
*/

/*SS: 8x64=512; 16x16=256, x16=4096; 8*512=4096
psg_envelope_level[envshape][psg_envstage & 63];
there are 8 shapes, what are the 64 values?

they should be how the wave changes with time, with a
precision of 64 steps
in sc68 it would be 256?



#define VOLTAGE_ZERO_LEVEL 0
#define VOLTAGE_FIXED_POINT 256
#define PSG_CHANNEL_AMPLITUDE 60

#define VFP VOLTAGE_FIXED_POINT
#define VZL VOLTAGE_ZERO_LEVEL
#define VA VFP*PSG_CHANNEL_AMPLITUDE


1000*VA/1000+VZL*VFP -> 1000* (256*60)/1000 + 0*256
YM master frequency in Atari ST:2Mhz
static u16 ymout[16*16*16] =
#include "io68/ym_fixed_vol.h"
*/
const int psg_envelope_level[8][64]={
    {1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP},
    {1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    1000*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,1*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,3*VA/1000+VZL*VFP,5*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,30*VA/1000+VZL*VFP,50*VA/1000+VZL*VFP,55*VA/1000+VZL*VFP,65*VA/1000+VZL*VFP,70*VA/1000+VZL*VFP,80*VA/1000+VZL*VFP,88*VA/1000+VZL*VFP,100*VA/1000+VZL*VFP,110*VA/1000+VZL*VFP,125*VA/1000+VZL*VFP,149*VA/1000+VZL*VFP,178*VA/1000+VZL*VFP,210*VA/1000+VZL*VFP,250*VA/1000+VZL*VFP,290*VA/1000+VZL*VFP,354*VA/1000+VZL*VFP,420*VA/1000+VZL*VFP,510*VA/1000+VZL*VFP,590*VA/1000+VZL*VFP,707*VA/1000+VZL*VFP,841*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP}};

#undef VFP
#undef VZL
#undef VA

DWORD psg_quantize_time(int,DWORD);
void psg_set_reg(int,BYTE,BYTE&);

DWORD psg_envelope_start_time=0xfffff000;
//---------------------------------------------------------------------------

#endif

#undef EXT
#undef INIT


#endif//!SSE_STRUCTURE_PSG_H