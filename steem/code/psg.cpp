/*---------------------------------------------------------------------------
FILE: psg.cpp
MODULE: emu
DESCRIPTION: Steem's Programmable Sound Generator (Yamaha 2149) and STE
DMA sound output emulation. Sound_VBL is the main function writing one
frame of sound to the output buffer. The I/O code isn't included here, see
ior.cpp and iow.cpp for the lowest level emulation.
---------------------------------------------------------------------------*/

#if defined(SSE_COMPILER_INCLUDED_CPP)
#pragma message("Included for compilation: psg.cpp")
#endif

#if defined(SSE_BUILD)

#ifdef IN_EMU
#define EXT
#define INIT(s) =s
#endif

#ifdef ENABLE_VARIABLE_SOUND_DAMPING
EXT int sound_variable_a INIT(32);
EXT int sound_variable_d INIT(208);
#endif
#if !defined(SOUND_DISABLE_INTERNAL_SPEAKER)
EXT bool sound_internal_speaker INIT(false);
#endif
#if defined(SSE_SOUND)
EXT int sound_freq INIT(44100),sound_comline_freq INIT(0),sound_chosen_freq INIT(44100);
#else
EXT int sound_freq INIT(50066),sound_comline_freq INIT(0),sound_chosen_freq INIT(50066);
#endif
#if defined(SSE_SOUND)
EXT BYTE sound_num_channels INIT(2),sound_num_bits INIT(16);
#elif defined(SSE_SOUND_NO_8BIT)
BYTE sound_num_channels INIT(2);
const BYTE sound_num_bits INIT(16);
#else
EXT BYTE sound_num_channels INIT(1),sound_num_bits INIT(8);
#endif
#if defined(SSE_VAR_RESIZE)
EXT BYTE sound_bytes_per_sample INIT(1);
#else
EXT int sound_bytes_per_sample INIT(1);
#endif
#if defined(SSE_SOUND_VOL_LOGARITHMIC)
EXT int MaxVolume INIT(10000);
#else
EXT DWORD MaxVolume INIT(0xffff);
#endif
EXT bool sound_low_quality INIT(0);
#if defined(SSE_SOUND_ENFORCE_RECOM_OPT)
const bool sound_write_primary=false;
#else
EXT bool sound_write_primary INIT( NOT_ONEGAME(0) ONEGAME_ONLY(true) );
#endif
EXT bool sound_click_at_start INIT(0);
EXT bool sound_record INIT(false);
EXT DWORD sound_record_start_time; //by timer variable = timeGetTime()
EXT int psg_write_n_screens_ahead INIT(3 UNIX_ONLY(+7) );

EXT int psg_voltage,psg_dv;

EXT BYTE psg_reg[16],psg_reg_data;

EXT FILE *wav_file INIT(NULL);

#if defined(ENABLE_LOGFILE) || defined(SHOW_WAVEFORM)
EXT DWORD min_write_time;
EXT DWORD play_cursor,write_cursor;
#endif

EXT int sound_buffer_length INIT(DEFAULT_SOUND_BUFFER_LENGTH);
EXT DWORD SoundBufStartTime;

#if SCREENS_PER_SOUND_VBL == 1
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
EXT BYTE temp_waveform_display[DEFAULT_SOUND_BUFFER_LENGTH];
EXT DWORD temp_waveform_play_counter;

#endif

#if !defined(SSE_YM2149_DISABLE_CAPTURE_FILE)
EXT FILE *psg_capture_file INIT(NULL);
EXT int psg_capture_cycle_base INIT(0);
EXT bool psg_always_capture_on_start INIT(0);
#endif

//not all are used
#define ONE_MILLION 1048576
#define TWO_MILLION 2097152
//two to the power of 21
#define TWO_TO_SIXTEEN 65536
#define TWO_TO_SEVENTEEN 131072
#define TWO_TO_EIGHTEEN 262144

bool sound_first_vbl=0;

void sound_record_to_wav(int,DWORD,bool,int*);

EXT BYTE dma_sound_control,dma_sound_mode;
EXT MEM_ADDRESS dma_sound_start=0,next_dma_sound_start=0,
            dma_sound_end=0,next_dma_sound_end=0;

WORD MicroWire_Mask=0x07ff;
WORD MicroWire_Data=0;
#if defined(SSE_TIMINGS_CPUTIMER64)
EXT COUNTER_VAR MicroWire_StartTime=0;
#else
int MicroWire_StartTime=0;
#endif
WORD dma_sound_internal_buf[4],dma_sound_last_word;
MEM_ADDRESS dma_sound_fetch_address;
#if defined(SSE_SOUND_DYNAMICBUFFERS2)
WORD *dma_sound_channel_buf=NULL;
int dma_sound_channel_buf_len=0; // variable
#else
WORD dma_sound_channel_buf[DMA_SOUND_BUFFER_LENGTH+16];
#endif
DWORD dma_sound_channel_buf_last_write_t;
#if defined(SSE_VAR_RESIZE)
EXT BYTE psg_reg_select;
#if defined(SSE_SOUND_ENFORCE_RECOM_OPT)
EXT const BYTE sound_time_method=1; // write cursor (hopefully!)
#else
EXT BYTE sound_time_method INIT(0);
#endif
#if defined(SSE_SOUND_FEWER_FILTERS)
EXT BYTE sound_mode INIT(SOUND_MODE_MONITOR),sound_last_mode INIT(SOUND_MODE_MONITOR);
#else
EXT BYTE sound_mode INIT(SOUND_MODE_CHIP),sound_last_mode INIT(SOUND_MODE_CHIP);
#endif
WORD dma_sound_mode_to_freq[4]={6258,12517,25033,50066},dma_sound_freq;
int dma_sound_output_countdown,dma_sound_samples_countdown;
BYTE dma_sound_internal_buf_len=0;
bool dma_sound_on_this_screen=0;
EXT BYTE dma_sound_mixer,dma_sound_volume;
EXT BYTE dma_sound_l_volume,dma_sound_r_volume;
EXT BYTE dma_sound_l_top_val,dma_sound_r_top_val;
#else
EXT int psg_reg_select;
EXT int sound_time_method INIT(0);
EXT int sound_mode INIT(SOUND_MODE_CHIP),sound_last_mode INIT(SOUND_MODE_CHIP);
int dma_sound_mode_to_freq[4]={6258,12517,25033,50066};
int dma_sound_freq,dma_sound_output_countdown,dma_sound_samples_countdown;
int dma_sound_internal_buf_len=0;
int dma_sound_on_this_screen=0;
int dma_sound_mixer=1,dma_sound_volume=40;
int dma_sound_l_volume=20,dma_sound_r_volume=20;
int dma_sound_l_top_val=128,dma_sound_r_top_val=128;
#endif
#if defined(SSE_SOUND_MICROWIRE)
#include "../../3rdparty/dsp/dsp.h"
#if defined(SSE_VAR_RESIZE)
BYTE dma_sound_bass=6; // 6 is neutral value
BYTE dma_sound_treble=6;
#else
int dma_sound_bass=6; // 6 is neutral value
int dma_sound_treble=6;
#endif
#if defined(SSE_SOUND_MICROWIRE_VOLUME)
TIirVolume MicrowireVolume[2]; 
#endif
TIirLowShelf MicrowireBass[2];
TIirHighShelf MicrowireTreble[2];
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
BYTE old_dma_sound_l_top_val,old_dma_sound_r_top_val;
#endif
#endif//microwire

#if defined(SSE_SOUND_DYNAMICBUFFERS)
int *psg_channels_buf=NULL;
int psg_channels_buf_len=0; // variable
#else
int psg_channels_buf[PSG_CHANNEL_BUF_LENGTH+16];
#endif

int psg_buf_pointer[3];
DWORD psg_tone_start_time[3];
char psg_noise[PSG_NOISE_ARRAY];

#define VFP VOLTAGE_FIXED_POINT
#define VZL VOLTAGE_ZERO_LEVEL
#define VA VFP*(PSG_CHANNEL_AMPLITUDE) //SS 15360, 1/3 of 46k  //256*60

const int psg_flat_volume_level[16]={0*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
                                      17*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,48*VA/1000+VZL*VFP,
                                      69*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,
                                      287*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP};


#if defined(SSE_YM2149_DYNAMIC_TABLE0) // to create file ym2149_fixed_vol.bin
 WORD fixed_vol_3voices[16][16][16]= 
#include "../../3rdparty/various/ym2149_fixed_vol.h" //more bloat...
#endif

#if defined(SSE_YM2149_FIX_ENV_TABLE) // must have values like the fixed volume table
//0	0	2	4	6	8	10	12	14	17	20	24	29	35	41	48	58	69	81	95	115	139	163	191	234	287	342	407	514	648	805	1000
const int psg_envelope_level[8][64]={
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP}};

#else

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
#endif

#if defined(SSE_YM2149_DELAY_RENDERING)
/*  Rendering is done later, we save the 5bit digital value instead of the 
    16bit "rendered" volume.
    TODO: something smarter than a dumb table?
*/
  
const BYTE psg_envelope_level3[8][64]={
    {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
     31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0},
    {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
     0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31},
    {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,0,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

#endif

#undef VFP
#undef VZL
#undef VA

DWORD psg_envelope_start_time=0xfffff000;

#undef EXT
#undef INIT


#endif

#define LOGSECTION LOGSECTION_SOUND
#if defined(SSE_YM2149_QUANTIZE_TRACE)
#define TRACE_PSG TRACE_LOG
#else
#define TRACE_PSG
#endif

#if defined(SSE_VID_RECORD_AVI) && defined(WIN32)
extern IDirectSoundBuffer *PrimaryBuf,*SoundBuf;
#endif

#if defined(SSE_SOUND_MICROWIRE_392)// for my half-arsed microwire emulation!
#define LOW_SHELF_FREQ 80 // officially 50 Hz
#define HIGH_SHELF_FREQ (min(10000,(int)sound_freq/2)) // officially  15 kHz
#elif defined(SSE_SOUND_MICROWIRE) 
#define LOW_SHELF_FREQ 80 // officially 50 Hz
#if defined(SSE_SOUND_MICROWIRE_TREBLE)
#define HIGH_SHELF_FREQ (min(2000,(int)dma_sound_freq/2)) // officially  15 kHz
#endif
#endif

#if defined(SSE_YM2149_RECORD_YM)
bool written_to_env_this_vbl=true;
#endif

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
HRESULT Sound_Start() // SS called by run() and other functions
{
#ifdef UNIX
#if !defined(SOUND_DISABLE_INTERNAL_SPEAKER)
  if (sound_internal_speaker){
    console_device=open("/dev/console",O_RDONLY | O_NDELAY,0);
    if (console_device==-1){
      printf("Couldn't open console for internal speaker output\n");
      sound_internal_speaker=false;
      GUIUpdateInternalSpeakerBut();
    }
  }
#endif
#endif
  if (sound_mode==SOUND_MODE_MUTE) return DS_OK;
  if (UseSound==0) return DSERR_GENERIC;  // Not initialised
  if (SoundActive()) return DS_OK;        // Already started

  if (fast_forward || slow_motion || runstate!=RUNSTATE_RUNNING) return DSERR_GENERIC;

  sound_first_vbl=true;
  dbg_log("SOUND: Starting sound buffers and initialising PSG variables");

  // Work out startup voltage
  int envshape=psg_reg[13] & 15;
  int flatlevel=0;
  for (int abc=0;abc<3;abc++){
    if ((psg_reg[8+abc] & BIT_4)==0){
      flatlevel+=psg_flat_volume_level[psg_reg[8+abc] & 15];
    }else if (envshape==b1011 || envshape==b1101){
      flatlevel+=psg_flat_volume_level[15]; //SS 15 = 1 anyway
    }
  }
  psg_voltage=flatlevel;psg_dv=0;

  WORD dma_l,dma_r;
  dma_sound_get_last_sample(&dma_l,&dma_r);
  int current_l=HIBYTE(flatlevel)+HIBYTE(dma_l),current_r=HIBYTE(flatlevel)+HIBYTE(dma_r);
#if !defined(SSE_SOUND_16BIT_CENTRED)
  if (sound_num_bits==16){
    current_l^=128;current_r^=128;
  }
#endif
  if (SoundStartBuffer((signed char)current_l,(signed char)current_r)!=DS_OK){
    return DDERR_GENERIC;
  }
  for (int n=PSG_NOISE_ARRAY-1;n>=0;n--) psg_noise[n]=(BYTE)random(2);//SS TODO

#ifdef ONEGAME
  // Make sure sound is still good(ish) if you are running below 80% speed
  OGExtraSamplesPerVBL=300;
  if (run_speed_ticks_per_second>1000){
    // Get the number of extra ms of sound per "second", change that to number
    // of samples, divide to get the number of samples per VBL and add extra.
    OGExtraSamplesPerVBL=((((run_speed_ticks_per_second-1000)*sound_freq)/1000)/shifter_freq)+300;
  }
#endif

  psg_time_of_start_of_buffer=0;
  psg_last_play_cursor=0;
//  psg_last_write_time=0; not used now?
  psg_time_of_last_vbl_for_writing=0;
  psg_time_of_next_vbl_for_writing=0;

/*
  psg_time_of_start_of_buffer=(0xffffffff-(sound_freq*4)) &(-PSG_BUF_LENGTH);
  psg_last_play_cursor=psg_time_of_start_of_buffer;
  psg_last_write_time=psg_time_of_start_of_buffer;
  psg_time_of_last_vbl_for_writing=psg_time_of_start_of_buffer;
  psg_time_of_next_vbl_for_writing=psg_time_of_start_of_buffer;
*/

  for (int abc=2;abc>=0;abc--){
    psg_buf_pointer[abc]=0;
    psg_tone_start_time[abc]=0;
  }

  for (int i=0;i<PSG_CHANNEL_BUF_LENGTH;i++) psg_channels_buf[i]=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);
  psg_envelope_start_time=0xff000000;

  if (sound_record){
    timer=timeGetTime();
    sound_record_start_time=timer+200; //start recording in 200ms time
    sound_record_open_file();
  }

#if defined(SSE_YM2149_MAMELIKE_ANTIALIAS)
  if(YM2149.AntiAlias)
  {
    YM2149.time_at_vbl_start=YM2149.m_cycles;
    YM2149.AntiAlias->init();
  }
#endif

  return DS_OK;
}
//---------------------------------------------------------------------------
#if !defined(SOUND_DISABLE_INTERNAL_SPEAKER)
void SoundStopInternalSpeaker()
{
  internal_speaker_sound_by_period(0);
}
#endif
//---------------------------------------------------------------------------
#if defined(SSE_SOUND_MICROWIRE)   // microwire this!

inline void Microwire(int channel,int &val) {

#if defined(SSE_SOUND_OPTION_DISABLE_DSP)
  if(DSP_DISABLED)
    return;
#endif

  double d_dsp_v=val;
  if(dma_sound_bass!=6)
    d_dsp_v=MicrowireBass[channel].FilterAudio(d_dsp_v,LOW_SHELF_FREQ,
    dma_sound_bass-6);
#if defined(SSE_SOUND_MICROWIRE_TREBLE)
  if(dma_sound_treble!=6)
    d_dsp_v=MicrowireTreble[channel].FilterAudio(d_dsp_v,HIGH_SHELF_FREQ
    ,dma_sound_treble-6);
#endif
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
  // we do it in the write loop
#elif defined(SSE_SOUND_MICROWIRE_VOLUME)
  if(dma_sound_volume<0x28
    ||dma_sound_l_volume<0x14 &&!channel 
    ||dma_sound_r_volume<0x14 &&channel)//3.6.1: 2 channels

    d_dsp_v=MicrowireVolume[channel].FilterAudio(d_dsp_v,
    dma_sound_volume-0x28+dma_sound_l_volume-0x14);
#endif

  val=d_dsp_v;//v3.7
}

#endif
//---------------------------------------------------------------------------
#if defined(SSE_SOUND_INLINE)
/*  We transform some macros into inline functions to make conditional 
    compilation easier.
    We use tricks to make the macro calls work without change
    TODO: check for performance
*/

inline void CalcVChip(int &v,int &dv,int *source_p) {
  //CALC_V_CHIP

//#define NBITS 5
//#define proportion 10

#if defined(SSE_SOUND_FILTER_HATARI)
/*  We add a filter based on Hatari code:
*/
/*
http://www.atari-forum.com/viewtopic.php?f=94&t=27076&start=25#p263207
Here are my specifically designed YM2149 and LMC1992 filters I applied to Hatari.
These filters were derived from first principles with circuit analysis, and represent
the signal at the Atari ST audio output.
*/

/*
This YM2149 filter provides the characteristic Atari ST sound at
44.1 KHz and 48KHz sample rates:
*/
/*--------------------------------------------------------------*/
/* Low Pass Filter routines.                                    */
/*--------------------------------------------------------------*/

/**
 * Get coefficients for different Fs (C10 is in ST only):
 * Wc = 2*M_PI*4895.1;
 * Fs = 44100;
 * warp = Wc/tanf((Wc/2)/Fs);
 * b = Wc/(warp+Wc);
 * a = (Wc-warp)/(warp+Wc);
 *
 * #define B_z (yms32)( 0.2667*(1<<15))
 * #define A_z (yms32)(-0.4667*(1<<15))
 *
 * y0 = (B_z*(x0 + x1) - A_z*y0) >> 15;
 * x1 = x0;
 *
 * The Lowpass Filter formed by C10 = 0.1 uF
 * and
 * R8=1k // 1k*(65119-46602)/65119 // R9=10k // R10=5.1k //
 * (R12=470)*(100=Q1_HFE) = 206.865 ohms when YM2149 is High
 * and
 * R8=1k // R9=10k // R10=5.1k // (R12=470)*(100=Q1_HFE)
 *                        = 759.1   ohms when YM2149 is Low
 * High corner is 1/(2*pi*(0.1*10e-6)*206.865) fc = 7693.7 Hz
 * Low  corner is 1/(2*pi*(0.1*10e-6)*795.1)   fc = 2096.6 Hz
 * Notes:
 * - using STF reference designators R8 R9 R10 C10 (from dec 1986 schematics)
 * - using corresponding numbers from psgstrep and psgquart
 * - 65119 is the largest value in Paulo's psgstrep table
 * - 46602 is the largest value in Paulo's psgquart table
 * - this low pass filter uses the highest cutoff frequency
 *   on the STf (a slightly lower frequency is reasonable).
 *
 * A first order lowpass filter with a high cutoff frequency
 * is used when the YM2149 pulls high, and a lowpass filter
 * with a low cutoff frequency is used when R8 pulls low.
 */

  if(sound_mode==SOUND_MODE_HATARI)
  {
    if(*source_p>v)
      v=(3*(*source_p + dv) + (v<<1)) >> 3;
    else
      v = ((*source_p + dv) + (6*v)) >> 3;
    dv=v;
  }
  else
#endif
#if defined(SSE_SOUND_FILTERS) 
  if(sound_mode==SOUND_MODE_MONITOR) 
  {
    v=SSE_SOUND_FILTER_MONITOR_V;
    dv=SSE_SOUND_FILTER_MONITOR_DV;
  }
  else 
#endif
  if (v!=*source_p || dv){                            
#if defined (DEBUG_BUILD) && defined(ENABLE_VARIABLE_SOUND_DAMPING)   // defined() for mingw
 // Boiler control, useless now (undef)
    v+=dv;             
    dv-=(v-(*source_p))*sound_variable_a >> 8;        
    dv*=sound_variable_d;           
    dv>>=8;   
#else    
    v+=dv;                                            
    dv-=(v-(*source_p)) >> 3;                         
    dv*=13;                                           
    dv>>=4; 
#endif
  }
}


inline void CalcVChip25Khz(int &v,int &dv,int *source_p) {
  //CALC_V_CHIP_25KHZ = low quality
#if defined(SSE_SOUND_FILTER_HATARI)
  if(sound_mode==SOUND_MODE_HATARI)
  {
    if(*source_p>v)
      v=(3*(*source_p + dv) + (v<<1)) >> 3;
    else
      v = ((*source_p + dv) + (6*v)) >> 3;
    dv=v;
  }
  else
#endif
#if defined(SSE_SOUND_FILTERS) 
  if(sound_mode==SOUND_MODE_MONITOR)
  {
    v=SSE_SOUND_FILTER_MONITOR_V;
    dv=SSE_SOUND_FILTER_MONITOR_DV;
  }
  else 
#endif
  if (v!=*source_p || dv){      
#ifdef ENABLE_VARIABLE_SOUND_DAMPING    
    v+=dv;             
    dv-=(v-(*source_p))*sound_variable_a >> 8;        
    dv*=sound_variable_d;           
    dv>>=8;   
#else   
    v+=dv;                                            
    dv-=((v-(*source_p)) *3) >>3;                         
    dv*=3;                                           
    dv>>=2;                                           
#endif
  }
}


inline void CalcVEmu(int &v,int *source_p) {
  //CALC_V_EMU  = direct
  v=*source_p;
}


#define CALC_V_CHIP             1
#define CALC_V_CHIP_25KHZ       2
#define CALC_V_EMU              3

#ifdef SHOW_WAVEFORM
  #define WAVEFORM_SET_VAL(v) (val=(v))
  #define WAVEFORM_ONLY(x) x
#else
  #define WAVEFORM_SET_VAL(v) v
  #define WAVEFORM_ONLY(x)
#endif

#ifdef WRITE_ONLY_SINE_WAVE //SS not defined
#define SINE_ONLY(s) s
//todo, one day...
#define WRITE_SOUND_LOOP(Alter_V)         \
	          while (c>0){                                                  \
                *(p++)=WAVEFORM_SET_VAL(BYTE(sin((double)t*(M_PI/64))*120+128)); \
                t++;                                                       \
                WAVEFORM_ONLY( temp_waveform_display[((int)(source_p-psg_channels_buf)+psg_time_of_last_vbl_for_writing) % MAX_temp_waveform_display_counter]=(BYTE)val; ) \
    	          *(source_p++)=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);                 \
                c--;    \
	          }

#else
#define SINE_ONLY(s)
#endif


inline void AlterV(int Alter_V,int &v,int &dv,int *source_p) {

#if defined(SSE_YM2149_DELAY_RENDERING)
/*
    each *source_p element is a 32bit integer made up like this:

    byte 0: channel A
    byte 1: channel B
    byte 2: channel C

    each channel byte is made up like this:
    bit 0-4: volume on 5bit
    bit 6: envelope (1) /fixed (0)
    we use the fixed volume table to render sound on 3 channels
    this table is 16x16x16, but envelope volume is coded on 5bit, so
    we need to interpolate somehow
    We can interpolate 15 values, so we have 15+16 but need 32
    31 -> 15
    30 -> 14.5
    29 -> 14
    ...
    2 -> 0.5
    1 -> 0 !
    0 -> 0
    We sacrifice the lowest value, of course. TODO: but we shifted the table already!
    We interpolate when the last bit isn't set.
    Because of that, we need to know if it was enveloped:
    11110 is max for fixed, 11111 is max for enveloped
    bit 6 is used for that (1: enveloped)
    PSG tunes sound distorted like in Hatari but that's correct
    eg Ace 2 on BIG demo
*/

  int source=*source_p; // *source_p will be used again, don't alter it

  if(OPTION_SAMPLED_YM  && YM2149.p_fixed_vol_3voices
#if defined(SSE_YM2149_MAMELIKE)
    && !OPTION_MAME_YM //already rendered
#endif
    )
  {
    BYTE index[3],interpolate[4];
    *(int*)interpolate=0;
    for(int abc=0;abc<3;abc++)
    {
      index[abc]=( ((source)>>1))&0xF; // 4bit volume

      interpolate[abc]=
        ( ((source)&BIT_6) && index[abc]>0 && !((source)&1)) ? 1 : 0;

      source>>=8;
      ASSERT( interpolate[abc]<=1 );
      ASSERT( index[abc]<=15 );
    }
    int vol=YM2149.p_fixed_vol_3voices[(16*16)*index[2]+16*index[1]+index[0]];
    if(*(int*)(&interpolate[0]))
    {
      ASSERT( !((index[0]-interpolate[0])&0x80) );
      ASSERT( !((index[1]-interpolate[1])&0x80) );
      ASSERT( !((index[2]-interpolate[2])&0x80) );
      int vol2=YM2149.p_fixed_vol_3voices[ (16*16)*(index[2]-interpolate[2])
        +16*(index[1]-interpolate[1])+(index[0]-interpolate[0])];
      vol= (int) sqrt( (float) vol * (float) vol2); 
    }
    source=vol;
  }//OPTION_SAMPLED_YM
#endif //#if defined(SSE_YM2149_DELAY_RENDERING)

#if defined(SSE_SOUND_MICROWIRE_MIXMODE_393)
/*  According to Atari doc, 0 means YM -12db and 2 means no YM, but it doesn't
    work like this. 
    On my STE, there's still YM sound at full voluem when it is 2,
    and 0 mutes the YM.
    Petari devised a HW hack/fix where '2' attenuates YM, we do the same in our 
    emulation.
    There are no commercial games using the (non-working) feature anyway.
*/
  if(ST_TYPE==STE && OPTION_MICROWIRE && dma_sound_mixer!=1)
  {
    if(dma_sound_mixer==2)
      source>>=3;
    else if(!dma_sound_mixer)
      source=0;
  }
#endif

  // Dispatch to the correct function  
  if(Alter_V==CALC_V_CHIP)                    
    CalcVChip(v,dv,&source);                 
  else if(Alter_V==CALC_V_CHIP_25KHZ)         
    CalcVChip25Khz(v,dv,&source);            
  else if(Alter_V==CALC_V_EMU)                
    CalcVEmu(v,&source);                     
}


/*  The function is called at VBL. The sounds have already been computed.
    The function adds an optional low-pass filter to PSG sound and adds
    PSG and DMA sound together. (Ground for improvement).
    It also applies Microwire filters.
    It shouldn't be important that it be inline.
*/

inline void WriteSoundLoop(int Alter_V, int* Out_P,int Size,int& c,int &val,
  int &v,int &dv,int **source_p,WORD**lp_dma_sound_channel,
  WORD**lp_max_dma_sound_channel) {

#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
/*  Try to avoid clicks when a program aggressively changes microwire volume
    (Sea of Colour).
    Now it won't work if a program does a lot of quick changes for effect.
*/
    if(!OPTION_MICROWIRE)
      ;
    else if(old_dma_sound_l_top_val<dma_sound_l_top_val)
      old_dma_sound_l_top_val++;
    else if(old_dma_sound_l_top_val>dma_sound_l_top_val)
      old_dma_sound_l_top_val--;
    if(old_dma_sound_r_top_val<dma_sound_r_top_val)
      old_dma_sound_r_top_val++;
    else if(old_dma_sound_r_top_val>dma_sound_r_top_val)
      old_dma_sound_r_top_val--;
#endif

  // check size once (hopefully optimised?)
  if(Size==sizeof(BYTE)) //8bit
  {
    while(c>0)
    {       
      AlterV(Alter_V,v,dv,*source_p);

      //LEFT-8bit
      val=v;
#if defined(SSE_OSD_CONTROL)
      if(dma_sound_on_this_screen&&(OSD_MASK3 & OSD_CONTROL_DMASND)) 
        TRACE_OSD("F%d %cV%d %d %d B%d T%d",dma_sound_freq,(dma_sound_mode & BIT_7)?'M':'S',dma_sound_volume,dma_sound_l_volume,dma_sound_r_volume,dma_sound_bass,dma_sound_treble);
#endif

#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
      if(! (d2_dpeek(FAKE_IO_START+20)>>15) )
#endif
        val+= (**lp_dma_sound_channel);                           
#if defined(SSE_SOUND_MICROWIRE)
      if(OPTION_MICROWIRE)
      { 
        Microwire(0,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
        if(dma_sound_l_top_val!=128||old_dma_sound_l_top_val!=dma_sound_l_top_val)
#else
        if(dma_sound_l_top_val!=128)
#endif
        {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
          val*=old_dma_sound_l_top_val;
          val/=128;
#else
          val*=dma_sound_l_top_val;
          val/=128;
#endif
        }
      }
#endif//#if defined(SSE_SOUND_MICROWIRE)
      if (val<VOLTAGE_FP(0))
        val=VOLTAGE_FP(0); 
      else if (val>VOLTAGE_FP(255))
        val=VOLTAGE_FP(255); 
#if defined(SSE_SOUND_MUTE_WHEN_INACTIVE)
      else if (MuteWhenInactive&&bAppActive==false) 
        val=0;
#endif
      *(BYTE*)*(BYTE**)Out_P=(BYTE)((val&0x00FF00)>>8);
      (*(BYTE**)Out_P)++;
      // stereo: do the same for right channel
      if(sound_num_channels==2){   
        //RIGHT-8bit
        val=v;
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
        if(! (d2_dpeek(FAKE_IO_START+20)>>15) ) 
#endif
          val+= (*(*lp_dma_sound_channel+1)); 
#if defined(SSE_SOUND_MICROWIRE)
        if(OPTION_MICROWIRE)
        {
          Microwire(1,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
          if(dma_sound_r_top_val!=128 ||old_dma_sound_r_top_val!=dma_sound_r_top_val)
#else
          if(dma_sound_r_top_val<128)
#endif
          {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
            val*=old_dma_sound_r_top_val;
            val/=128;
#else
            val*=dma_sound_r_top_val;
            val/=128;
#endif
          }
        }
#endif//#if defined(SSE_SOUND_MICROWIRE)
        if(val<VOLTAGE_FP(0))
          val=VOLTAGE_FP(0); 
        else if (val>VOLTAGE_FP(255))
          val=VOLTAGE_FP(255); 
#if defined(SSE_SOUND_MUTE_WHEN_INACTIVE)
        else if (MuteWhenInactive&&bAppActive==false) 
          val=0;
#endif
        *(BYTE*)*(BYTE**)Out_P=(BYTE)((val&0x00FF00)>>8);
        (* (BYTE**)Out_P )++;
      }//right channel 
      WAVEFORM_ONLY(temp_waveform_display[((int)(*source_p-psg_channels_buf)+psg_time_of_last_vbl_for_writing) % MAX_temp_waveform_display_counter]=WORD_B_1(&val)); 
      *(*source_p)++=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);
      if(*lp_dma_sound_channel<*lp_max_dma_sound_channel) 
        *lp_dma_sound_channel+=2;
      c--;                                                          
    }//wend
  }
  else //16bit
  {
    while(c>0)
    {       
      AlterV(Alter_V,v,dv,*source_p);

      //LEFT-16bit
      val=v; //inefficient?
#if defined(SSE_SOUND_16BIT_CENTRED)
      char dma_sample=**lp_dma_sound_channel; 
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
      if(! (d2_dpeek(FAKE_IO_START+20)>>15) )
#endif
#if defined(SSE_CARTRIDGE_BAT)
        val+=(SSEConfig.mv16 || SSEConfig.mr16
#if defined(SSE_DONGLE_PROSOUND) && defined(SSE_BUGFIX_394)
          ||(DONGLE_ID==TDongle::PROSOUND)
#endif
        )
        ? (**lp_dma_sound_channel)
        : (dma_sample*DMA_SOUND_MULTIPLIER);
#else
        val+=dma_sample*DMA_SOUND_MULTIPLIER;
#endif
#else
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
      if(! (d2_dpeek(FAKE_IO_START+20)>>15) )
#endif
        val+= (**lp_dma_sound_channel);    
#endif //dc                      
#if defined(SSE_SOUND_MICROWIRE)
      if(OPTION_MICROWIRE)
      {
       // TRACE_OSD("%d %d %d",val,dma_sound_l_top_val,old_dma_sound_l_top_val);
        Microwire(0,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
        if(dma_sound_l_top_val!=128||old_dma_sound_l_top_val!=dma_sound_l_top_val)
#else
        if(dma_sound_l_top_val!=128)
#endif
        {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
          val*=old_dma_sound_l_top_val;
          val/=128;
#else
          val*=dma_sound_l_top_val;
          val/=128;
#endif
        }
      }
#endif//#if defined(SSE_SOUND_MICROWIRE)

#if defined(SSE_SOUND_16BIT_CENTRED)
      if(val>32767)
        val=32767;
#else
      if (val<VOLTAGE_FP(0))
        val=VOLTAGE_FP(0); 
      else if (val>VOLTAGE_FP(255))
        val=VOLTAGE_FP(255); 
#endif
#if defined(SSE_SOUND_MUTE_WHEN_INACTIVE)
      else if (MuteWhenInactive&&bAppActive==false) 
        val=0;
#endif
#if defined(SSE_SOUND_16BIT_CENTRED)
      *(WORD*)*(WORD**)Out_P=((WORD)val);
#else
      *(WORD*)*(WORD**)Out_P=((WORD)val) ^ MSB_W;
#endif//dc
      (*(WORD**)Out_P)++;
      // stereo: do the same for right channel
      if(sound_num_channels==2){    
        //RIGHT-16bit
        val=v;
#if defined(SSE_SOUND_16BIT_CENTRED)
        char dma_sample=*(*lp_dma_sound_channel+1); 
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
        if(! (d2_dpeek(FAKE_IO_START+20)>>15) )
#endif
#if defined(SSE_CARTRIDGE_BAT)
        val+=(SSEConfig.mv16 || SSEConfig.mr16
#if defined(SSE_DONGLE_PROSOUND) && defined(SSE_BUGFIX_394)
          ||(DONGLE_ID==TDongle::PROSOUND)
#endif
        )
        ? (*(*lp_dma_sound_channel+1))
        : (dma_sample*DMA_SOUND_MULTIPLIER);
#else
          val+=dma_sample*DMA_SOUND_MULTIPLIER;
#endif
#else
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
        if(! (d2_dpeek(FAKE_IO_START+20)>>15) ) 
#endif
          val+= (*(*lp_dma_sound_channel+1)); 
#endif//dc
#if defined(SSE_SOUND_MICROWIRE)
        if(OPTION_MICROWIRE)
        {
          Microwire(1,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
          if(dma_sound_r_top_val<128 ||old_dma_sound_r_top_val!=dma_sound_r_top_val)
#else
          if(dma_sound_r_top_val<128)
#endif
          {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
            val*=old_dma_sound_r_top_val;
            val/=128;
#else
            val*=dma_sound_r_top_val;
            val/=128;
#endif
          }
        }
#endif
#if defined(SSE_SOUND_16BIT_CENTRED)
        if(val>32767)
          val=32767;
#else
        if(val<VOLTAGE_FP(0))
          val=VOLTAGE_FP(0); 
        else if (val>VOLTAGE_FP(255))
          val=VOLTAGE_FP(255); 
#endif
#if defined(SSE_SOUND_MUTE_WHEN_INACTIVE)
        else if (MuteWhenInactive&&bAppActive==false) 
          val=0;
#endif
#if defined(SSE_SOUND_16BIT_CENTRED)
        *(WORD*)*(WORD**)Out_P=((WORD)val);
#else
        *(WORD*)*(WORD**)Out_P=((WORD)val) ^ MSB_W;
#endif//dc
        (*(WORD**)Out_P)++;
      }//right channel 
      WAVEFORM_ONLY(temp_waveform_display[((int)(*source_p-psg_channels_buf)+psg_time_of_last_vbl_for_writing) % MAX_temp_waveform_display_counter]=WORD_B_1(&val)); 
      *(*source_p)++=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);
      if(*lp_dma_sound_channel<*lp_max_dma_sound_channel) 
        *lp_dma_sound_channel+=2;
      c--;                                                          
    }//wend
  }    
}
#define WRITE_SOUND_LOOP(Alter_V,Out_P,Size,GetSize) WriteSoundLoop(Alter_V,(int*)&Out_P,sizeof(Size),c,val,v,dv,&source_p,&lp_dma_sound_channel,&lp_max_dma_sound_channel);

#define WRITE_TO_WAV_FILE_B 1 
#define WRITE_TO_WAV_FILE_W 2 


inline void SoundRecord(int Alter_V, int Write,int& c,int &val,
  int &v,int &dv,int **source_p,WORD**lp_dma_sound_channel,
  WORD**lp_max_dma_sound_channel,FILE* wav_file) {

  while(c>0)
  {       
    AlterV(Alter_V,v,dv,*source_p);

    val=v;

#if defined(SSE_SOUND_16BIT_CENTRED)
    if(RENDER_SIGNED_SAMPLES)
    {
      char dma_sample=**lp_dma_sound_channel; 
#if defined(SSE_CARTRIDGE_BAT)
      val+=(SSEConfig.mv16 || SSEConfig.mr16)
        ? (**lp_dma_sound_channel)
        : (dma_sample*DMA_SOUND_MULTIPLIER);
#else
      val+=dma_sample*DMA_SOUND_MULTIPLIER;
#endif
    }
    else
#endif
    val+= (**lp_dma_sound_channel);  

#if defined(SSE_SOUND_MICROWIRE)
    if(OPTION_MICROWIRE)
    {
      Microwire(0,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
      if(dma_sound_l_top_val!=128||old_dma_sound_l_top_val!=dma_sound_l_top_val)
#else
      if(dma_sound_l_top_val<128)
#endif
      {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
        val*=old_dma_sound_l_top_val; // changed in live sound loop
        val/=128;
#else
        val*=dma_sound_l_top_val;
        val/=128;
#endif
      }
    }
#endif
#if defined(SSE_SOUND_16BIT_CENTRED)
    if(val>32767)
      val=32767;
#else
    if (val<VOLTAGE_FP(0))
      val=VOLTAGE_FP(0); 
    else if (val>VOLTAGE_FP(255))
      val=VOLTAGE_FP(255); 
#endif
    if(Write==WRITE_TO_WAV_FILE_B) 
      fputc(BYTE(WORD_B_1(&(val))),wav_file);
    else 
    {
#if !defined(SSE_SOUND_16BIT_CENTRED)
      val^=MSB_W;
#endif//dc
      fputc(LOBYTE(val),wav_file);
      fputc(HIBYTE(val),wav_file);
    }

    if(sound_num_channels==2){ // RIGHT CHANNEL

      val=v; // restore val! 

#if defined(SSE_SOUND_16BIT_CENTRED)
      if(RENDER_SIGNED_SAMPLES)
      {
        char dma_sample=*(*lp_dma_sound_channel+1); 
#if defined(SSE_CARTRIDGE_BAT)
        val+=(SSEConfig.mv16 || SSEConfig.mr16)
          ? (*(*lp_dma_sound_channel+1))
          : (dma_sample*DMA_SOUND_MULTIPLIER);
#else
        val+=dma_sample*DMA_SOUND_MULTIPLIER;
#endif
      }
      else
#endif
      val+= (*(*lp_dma_sound_channel+1)); 

#if defined(SSE_SOUND_MICROWIRE)
      if(OPTION_MICROWIRE)
      {
        Microwire(1,val);
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
        if(dma_sound_r_top_val!=128||old_dma_sound_r_top_val!=dma_sound_r_top_val)
#else
        if(dma_sound_r_top_val<128)
#endif
        {
#if defined(SSE_SOUND_MICROWIRE_VOLUME_SLOW)
          val*=old_dma_sound_r_top_val;
          val/=128;
#else
          val*=dma_sound_r_top_val;
          val/=128;
#endif
        }
      }
#endif
#if defined(SSE_SOUND_16BIT_CENTRED)
      if(val>32767)
        val=32767;
#else
      if(val<VOLTAGE_FP(0))
        val=VOLTAGE_FP(0); 
      else if (val>VOLTAGE_FP(255))
        val=VOLTAGE_FP(255); 
#endif
      if(Write==WRITE_TO_WAV_FILE_B) 
        fputc(BYTE(WORD_B_1(&(val))),wav_file);
      else 
      {
#if !defined(SSE_SOUND_16BIT_CENTRED)
        val^=MSB_W;
#endif //dc
        fputc(LOBYTE(val),wav_file);
        fputc(HIBYTE(val),wav_file);
      }

    }//right
    (*source_p)++;// don't zero! (or mute when recording)
    

    SINE_ONLY( t++ );
    if(*lp_dma_sound_channel<*lp_max_dma_sound_channel) 
      *lp_dma_sound_channel+=2;
    c--;   
  }//wend
}
#define SOUND_RECORD(Alter_V,WRITE) SoundRecord(Alter_V,WRITE,c,val,v,dv,&source_p,&lp_dma_sound_channel,&lp_max_dma_sound_channel,wav_file)

#else // original macros

#ifdef ENABLE_VARIABLE_SOUND_DAMPING

#define CALC_V_CHIP  \
                if (v!=*source_p || dv){                            \
                  v+=dv;                                            \
                  dv-=(v-(*source_p))*sound_variable_a >> 8;        \
                  dv*=sound_variable_d;                                           \
                  dv>>=8;                                           \
                }

#define CALC_V_CHIP_25KHZ  \
                if (v!=*source_p || dv){                            \
                  v+=dv;                                            \
                  dv-=(v-(*source_p))*sound_variable_a >> 8;        \
                  dv*=sound_variable_d;                                           \
                  dv>>=8;                                           \
                }

#else

#define CALC_V_CHIP  \
                if (v!=*source_p || dv){                            \
                  v+=dv;                                            \
                  dv-=(v-(*source_p)) >> 3;                         \
                  dv*=13;                                           \
                  dv>>=4;                                           \
                }

#define CALC_V_CHIP_25KHZ  \
                if (v!=*source_p || dv){                            \
                  v+=dv;                                            \
                  dv-=((v-(*source_p)) *3) >>3;                         \
                  dv*=3;                                           \
                  dv>>=2;                                           \
                }

//60, C0

#endif

#define CALC_V_EMU  v=*source_p;

#ifdef SHOW_WAVEFORM
  #define WAVEFORM_SET_VAL(v) (val=(v))
  #define WAVEFORM_ONLY(x) x
#else
  #define WAVEFORM_SET_VAL(v) v
  #define WAVEFORM_ONLY(x)
#endif



#ifdef WRITE_ONLY_SINE_WAVE

#define SINE_ONLY(s) s

#define WRITE_SOUND_LOOP(Alter_V)         \
	          while (c>0){                                                  \
                *(p++)=WAVEFORM_SET_VAL(BYTE(sin((double)t*(M_PI/64))*120+128)); \
                t++;                                                       \
                WAVEFORM_ONLY( temp_waveform_display[((int)(source_p-psg_channels_buf)+psg_time_of_last_vbl_for_writing) % MAX_temp_waveform_display_counter]=(BYTE)val; ) \
    	          *(source_p++)=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);                 \
                c--;    \
	          }

#else

#define SINE_ONLY(s)

#define WRITE_SOUND_LOOP(Alter_V,Out_P,Size,GetSize)         \
	          while (c>0){                                                  \
              Alter_V                                                     \
              val=v + *lp_dma_sound_channel;                           \
  	          if (val<VOLTAGE_FP(0)){                                     \
                val=VOLTAGE_FP(0); \
      	      }else if (val>VOLTAGE_FP(255)){                            \
        	      val=VOLTAGE_FP(255);                    \
  	          }                                                            \
              *(Out_P++)=Size(GetSize(&val)); \
              if (sound_num_channels==2){         \
                val=v + *(lp_dma_sound_channel+1);                                            \
                if (val<VOLTAGE_FP(0)){                                     \
                  val=VOLTAGE_FP(0); \
                }else if (val>VOLTAGE_FP(255)){                            \
                  val=VOLTAGE_FP(255);                    \
                }                                                            \
                *(Out_P++)=Size(GetSize(&val)); \
              }     \
    	        WAVEFORM_ONLY(temp_waveform_display[((int)(source_p-psg_channels_buf)+psg_time_of_last_vbl_for_writing) % MAX_temp_waveform_display_counter]=WORD_B_1(&val)); \
  	          *(source_p++)=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL);                 \
              if (lp_dma_sound_channel<lp_max_dma_sound_channel) lp_dma_sound_channel+=2; \
      	      c--;                                                          \
	          }

#endif

#define WRITE_TO_WAV_FILE_B(val) fputc(BYTE(WORD_B_1(&(val))),wav_file);
#define WRITE_TO_WAV_FILE_W(val) val^=MSB_W;fputc(LOBYTE(val),wav_file);fputc(HIBYTE(val),wav_file);

#define SOUND_RECORD(Alter_V,WRITE)         \
            while (c>0){                        \
              Alter_V;                          \
              val=v + *lp_dma_sound_channel;                           \
              if (val<VOLTAGE_FP(0)){                                     \
                val=VOLTAGE_FP(0); \
              }else if (val>VOLTAGE_FP(255)){                            \
                val=VOLTAGE_FP(255); \
              }                                                            \
              WRITE(val); \
              if (sound_num_channels==2){         \
                val=v + *(lp_dma_sound_channel+1);                           \
                if (val<VOLTAGE_FP(0)){                                     \
                  val=VOLTAGE_FP(0); \
                }else if (val>VOLTAGE_FP(255)){                            \
                  val=VOLTAGE_FP(255); \
                }                                                            \
                WRITE(val); \
              }  \
              source_p++;                       \
              SINE_ONLY( t++ );                                   \
              if (lp_dma_sound_channel<lp_max_dma_sound_channel) lp_dma_sound_channel+=2; \
              c--;                                          \
            }

#endif//inline/macros
//---------------------------------------------------------------------------
void sound_record_to_wav(int c,DWORD SINE_ONLY(t),bool chipmode,int *source_p)
{
  if (timer<sound_record_start_time) return;

  int v=psg_voltage,dv=psg_dv; //restore from last time
  WORD *lp_dma_sound_channel=dma_sound_channel_buf;
  WORD *lp_max_dma_sound_channel=dma_sound_channel_buf+dma_sound_channel_buf_last_write_t;
  int val;
  if (sound_num_bits==8){
    if (chipmode){
      if (sound_low_quality==0){
        SOUND_RECORD(CALC_V_CHIP,WRITE_TO_WAV_FILE_B);
      }else{
        SOUND_RECORD(CALC_V_CHIP_25KHZ,WRITE_TO_WAV_FILE_B);
      }
    }else{
      SOUND_RECORD(CALC_V_EMU,WRITE_TO_WAV_FILE_B);
    }
  }else{
    if (chipmode){
      if (sound_low_quality==0){
        SOUND_RECORD(CALC_V_CHIP,WRITE_TO_WAV_FILE_W);
      }else{
        SOUND_RECORD(CALC_V_CHIP_25KHZ,WRITE_TO_WAV_FILE_W);
      }
    }else{
      SOUND_RECORD(CALC_V_EMU,WRITE_TO_WAV_FILE_W);
    }
  }
}

#ifdef VC_BUILD
#pragma warning (disable: 4018) //signed/unsigned mismatch
#endif

HRESULT Sound_VBL()
{
#if SCREENS_PER_SOUND_VBL != 1 //SS it is 1
  static int screens_countdown=SCREENS_PER_SOUND_VBL;
  screens_countdown--;if (screens_countdown>0) return DD_OK;
  screens_countdown=SCREENS_PER_SOUND_VBL;
  cpu_time_of_last_sound_vbl=ABSOLUTE_CPU_TIME;
#endif
#if !defined(SOUND_DISABLE_INTERNAL_SPEAKER)
  if (sound_internal_speaker){
    static double op=0;
    int abc,chan=-1,max_vol=0,vol;
    // Find loudest channel
    for (abc=0;abc<3;abc++){
      if ((psg_reg[PSGR_MIXER] & (1 << abc))==0){ // Channel enabled in mixer
        vol=(psg_reg[PSGR_AMPLITUDE_A+abc] & 15);
        if (vol>max_vol){
          chan=abc;
          max_vol=vol;
        }
      }
    }
    if (chan==-1){ //no sound
      internal_speaker_sound_by_period(0);
      op=0;
    }else{
      double p=((((int)psg_reg[chan*2+1] & 0xf) << 8) + psg_reg[chan*2]);
      p*=(1193181.0/125000.0);
      if (op!=p){
        op=p;
        internal_speaker_sound_by_period((int)p);
      }
    }
  }
#endif
#if !defined(SSE_YM2149_DISABLE_CAPTURE_FILE)
  if (psg_capture_file){
    psg_capture_check_boundary();
  }
#endif
  // This just clears up some clicks when Sound_VBL is called very soon after Sound_Start
  if (sound_first_vbl
#ifdef SSE_BUGFIX_394
    && !OPTION_MAME_YM
#endif
    ){
    sound_first_vbl=0;
    return DS_OK;
  }

  if (sound_mode==SOUND_MODE_MUTE) return DS_OK;

  if (UseSound==0) return DSERR_GENERIC;  // Not initialised
  if (SoundActive()==0) return DS_OK;        // Not started

  dbg_log("");
  dbg_log("SOUND: Start of Sound_VBL");

  void *DatAdr[2]={NULL,NULL};
  DWORD LockLength[2]={0,0};
  DWORD s_time,write_time_1,write_time_2;
  HRESULT Ret;

  int *source_p;

  DWORD n_samples_per_vbl=(sound_freq*SCREENS_PER_SOUND_VBL)/shifter_freq;
  dbg_log(EasyStr("SOUND: Calculating time; psg_time_of_start_of_buffer=")+psg_time_of_start_of_buffer);


  s_time=SoundGetTime();
  //we have data from time_of_last_vbl+PSG_WRITE_N_SCREENS_AHEAD*n_samples_per_vbl up to
  //wherever we want

  write_time_1=psg_time_of_last_vbl_for_writing; //3 screens ahead of where the cursor was
//  write_time_1=max(write_time_1,min_write_time); //minimum time for new write

  write_time_2=max(write_time_1+(n_samples_per_vbl+PSG_WRITE_EXTRA),
                   s_time+(n_samples_per_vbl+PSG_WRITE_EXTRA));
  if ((write_time_2-write_time_1)>PSG_CHANNEL_BUF_LENGTH){
    write_time_2=write_time_1+PSG_CHANNEL_BUF_LENGTH;
  }

//  psg_last_write_time=write_time_2;

  DWORD time_of_next_vbl_to_write=max(s_time+n_samples_per_vbl*psg_write_n_screens_ahead,psg_time_of_next_vbl_for_writing);
  if (time_of_next_vbl_to_write>s_time+n_samples_per_vbl*(psg_write_n_screens_ahead+2)){  //
    time_of_next_vbl_to_write=s_time+n_samples_per_vbl*(psg_write_n_screens_ahead+2);     // new bit added by Ant 9/1/2001 to stop the sound lagging behind
  }                                                                                    // get rid of it if it is causing problems

  dbg_log(EasyStr("   writing from ")+write_time_1+" to "+write_time_2+"; current play cursor at "+s_time+" ("+play_cursor+"); minimum write at "+min_write_time+" ("+write_cursor+")");

//  log_write(EasyStr("writing ")+(write_time_1-s_time)+" samples ahead of play cursor, "+(write_time_1-min_write_time)+" ahead of min write");

#ifdef SHOW_WAVEFORM
  temp_waveform_display_counter=write_time_1 MOD_PSG_BUF_LENGTH;
  temp_waveform_play_counter=play_cursor;
#endif
  dbg_log("SOUND: Working out data up to the end of this VBL plus a bit more for all channels");
  TRACE_PSG("VBL finishing sound buffers from %d to %d (%d)+ extra %d\n",psg_time_of_last_vbl_for_writing,time_of_next_vbl_to_write,time_of_next_vbl_to_write-psg_time_of_last_vbl_for_writing,PSG_WRITE_EXTRA);
#if defined(SSE_YM2149_MAMELIKE)
  if(OPTION_MAME_YM)
  {
#if defined(SSE_YM2149_MAMELIKE_ANTIALIAS)
    YM2149.psg_write_buffer(time_of_next_vbl_to_write,true);
//    TRACE_OSD("%d",YM2149.frame_samples); // should be 882 @50hz
    YM2149.frame_samples=0;
    YM2149.time_at_vbl_start=YM2149.m_cycles;//YM2149.time_of_last_sample;
#else
    YM2149.psg_write_buffer(time_of_next_vbl_to_write);
#endif
    // Fill extra buffer, but without advancing the YM emu
    for(int i=max(1,psg_buf_pointer[0]);i<psg_buf_pointer[0]+PSG_WRITE_EXTRA;i++)
      *(psg_channels_buf+i)=*(psg_channels_buf+i-1);
  }
  else
#endif
  {//tmp
  //TRACE_OSD("%d",max(max(psg_buf_pointer[2],psg_buf_pointer[1]),psg_buf_pointer[0]));
  for (int abc=2;abc>=0;abc--){
    psg_write_buffer(abc,time_of_next_vbl_to_write+PSG_WRITE_EXTRA);
  }
  }//tmp
  if (dma_sound_on_this_screen){
#if defined(SSE_CARTRIDGE_BAT)
    if(SSEConfig.mv16)
      dma_sound_freq=sound_freq;
#endif
#if (PSG_WRITE_EXTRA>0)
    WORD w[2]={dma_sound_channel_buf[dma_sound_channel_buf_last_write_t-2],dma_sound_channel_buf[dma_sound_channel_buf_last_write_t-1]};
    for (int i=0;i<PSG_WRITE_EXTRA;i++){
      if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) break;
      dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w[0];
      dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w[1];
    }
#endif
#if defined(SSE_CARTRIDGE_BAT)
  }else if(SSEConfig.mv16){
    dma_sound_channel_buf[0]=dma_sound_channel_buf[1]=dma_sound_last_word;
    dma_sound_channel_buf_last_write_t=0;
#endif
  }else{
    WORD w1,w2;
    dma_sound_get_last_sample(&w1,&w2);
    dma_sound_channel_buf[0]=w1;
    dma_sound_channel_buf[1]=w2;
    dma_sound_channel_buf_last_write_t=0;
  }

  // write_time_1 and 2 are sample variables, convert to bytes
  DWORD StartByte=(write_time_1 MOD_PSG_BUF_LENGTH)*sound_bytes_per_sample;
  DWORD NumBytes=((write_time_2-write_time_1)+1)*sound_bytes_per_sample;
  dbg_log(EasyStr("SOUND: Trying to lock from ")+StartByte+", length "+NumBytes);
  Ret=SoundLockBuffer(StartByte,NumBytes,&DatAdr[0],&LockLength[0],&DatAdr[1],&LockLength[1]);
  if (Ret!=DSERR_BUFFERLOST){
    if (Ret!=DS_OK){
      log_write("SOUND: Lock totally failed, disaster!");
      return SoundError("Lock for PSG Buffer Failed",Ret);
    }
    dbg_log(EasyStr("SOUND: Locked lengths ")+LockLength[0]+", "+LockLength[1]);
    int i=min(max(int(write_time_1-psg_time_of_last_vbl_for_writing),0),PSG_CHANNEL_BUF_LENGTH-10);
    int v=psg_voltage,dv=psg_dv; //restore from last time
    dbg_log(EasyStr("SOUND: Zeroing channels buffer up to ")+i);
    for (int j=0;j<i;j++){
      ASSERT(j<PSG_CHANNEL_BUF_LENGTH);
      psg_channels_buf[j]=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL); //zero the start of the buffer
    }
    source_p=psg_channels_buf+i;
    int samples_left_in_buffer=max(PSG_CHANNEL_BUF_LENGTH-i,0);
    int countdown_to_storing_values=max((int)(time_of_next_vbl_to_write-write_time_1),0);
    //this is set when we are counting down to the start time of the next write
    bool store_values=false,chipmode=bool((sound_mode==SOUND_MODE_EMULATED) ? false:true);
#if !defined(SSE_SOUND_FEWER_FILTERS)
    if (sound_mode==SOUND_MODE_SHARPSAMPLES) chipmode=(psg_reg[PSGR_MIXER] & b00111111)!=b00111111;
    if (sound_mode==SOUND_MODE_SHARPCHIP)    chipmode=(psg_reg[PSGR_MIXER] & b00111111)==b00111111;
#endif
    if (sound_record){

#if defined(SSE_YM2149_RECORD_YM)
/*  Each VBL we dump PSG registers. We must write the envelope register
    only if it was written to (even same value), otherwise we write $FF.
*/
      if(OPTION_SOUND_RECORD_FORMAT==TOption::SoundFormatYm)
      {
        // each VBL write 14 regs
        fwrite(psg_reg,sizeof(BYTE),13,wav_file);
        BYTE env=written_to_env_this_vbl?psg_reg[13]:0xFF;
        fwrite(&env,sizeof(BYTE),1,wav_file);
        written_to_env_this_vbl=false;
      }
      else
#endif
      sound_record_to_wav(countdown_to_storing_values,write_time_1,chipmode,source_p);
    }

//    TRACE_OSD("snd mode %d chip %d",sound_mode,chipmode);

#ifdef WRITE_ONLY_SINE_WAVE
    DWORD t=write_time_1;
#endif

    int val;
    dbg_log("SOUND: Starting to write to buffers");
    WORD *lp_dma_sound_channel=dma_sound_channel_buf;
    WORD *lp_max_dma_sound_channel=dma_sound_channel_buf+dma_sound_channel_buf_last_write_t;
    BYTE *pb;
    WORD *pw;
    for (int n=0;n<2;n++){
      if (DatAdr[n]){
        pb=(BYTE*)(DatAdr[n]);
        pw=(WORD*)(DatAdr[n]);
        int c=min(int(LockLength[n]/sound_bytes_per_sample),samples_left_in_buffer),oc=c;
        if (c>countdown_to_storing_values){
          c=countdown_to_storing_values;
          oc-=countdown_to_storing_values;
          store_values=true;
        }
        for (;;){
          if (sound_num_bits==8){
            if (chipmode){
              if (sound_low_quality==0){
                WRITE_SOUND_LOOP(CALC_V_CHIP,pb,BYTE,DWORD_B_1);
              }else{
                WRITE_SOUND_LOOP(CALC_V_CHIP_25KHZ,pb,BYTE,DWORD_B_1);
              }
            }else{
              WRITE_SOUND_LOOP(CALC_V_EMU,pb,BYTE,DWORD_B_1);
            }
          }else{
            if (chipmode){
              if (sound_low_quality==0){
                WRITE_SOUND_LOOP(CALC_V_CHIP,pw,WORD,MSB_W ^ DWORD_W_0);
              }else{
                WRITE_SOUND_LOOP(CALC_V_CHIP_25KHZ,pw,WORD,MSB_W ^ DWORD_W_0);
              }
            }else{
              WRITE_SOUND_LOOP(CALC_V_EMU,pw,WORD,MSB_W ^ DWORD_W_0);
            }
          }

          if (store_values){
            c=oc;
            psg_voltage=v;
            psg_dv=dv;
            store_values=false;
            countdown_to_storing_values=0x7fffffff; //don't store the values again.
          }else{
            countdown_to_storing_values-=LockLength[n]/sound_bytes_per_sample;
            break;
          }
        }
        samples_left_in_buffer-=LockLength[n]/sound_bytes_per_sample;
      }
    }

#if defined(SSE_VID_RECORD_AVI) 
    if(video_recording&&SoundBuf&&pAviFile&&pAviFile->Initialised)
    {
      VERIFY( pAviFile->AppendSound(DatAdr[0],LockLength[0])==0 );
    }
#endif
    SoundUnlock(DatAdr[0],LockLength[0],DatAdr[1],LockLength[1]);
    ASSERT(source_p <= (psg_channels_buf+PSG_CHANNEL_BUF_LENGTH));
    while (source_p < (psg_channels_buf+PSG_CHANNEL_BUF_LENGTH)){
      *(source_p++)=VOLTAGE_FP(VOLTAGE_ZERO_LEVEL); //zero the rest of the buffer
    }
  }
  psg_buf_pointer[0]=0;
  psg_buf_pointer[1]=0;
  psg_buf_pointer[2]=0;

  psg_time_of_last_vbl_for_writing=time_of_next_vbl_to_write;

  psg_time_of_next_vbl_for_writing=max(s_time+n_samples_per_vbl*(psg_write_n_screens_ahead+1),
                                       time_of_next_vbl_to_write+n_samples_per_vbl);
  psg_time_of_next_vbl_for_writing=min(psg_time_of_next_vbl_for_writing,
                                       s_time+(PSG_BUF_LENGTH/2));
  dbg_log(EasyStr("SOUND: psg_time_of_next_vbl_for_writing=")+psg_time_of_next_vbl_for_writing);

  psg_n_samples_this_vbl=psg_time_of_next_vbl_for_writing-psg_time_of_last_vbl_for_writing;

  dbg_log("SOUND: End of Sound_VBL");
  dbg_log("");
  return DS_OK;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*                                DMA SOUND                                  */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void dma_sound_set_control(BYTE io_src_b)
{
/*
  DMA-Sound Control Register:

    $FFFF8900  0 0 0 0 0 0 0 0 0 0 0 0 0 0 X X

Writing a "00" to the last 2 bits terminate DMA sound replay.

Bit 0 controls Replay off/on, Bit 1 controls Loop off/on (0=off, 1=on).
*/
#if defined(SSE_DEBUG)
  TRACE_LOG("%d DMA sound ",ACT);
  if(io_src_b&BIT_1)
    TRACE_LOG("loop ");
#endif

  if ((dma_sound_control & BIT_0) && (io_src_b & BIT_0)==0){  //Stopping
    TRACE_LOG("stop");
    dma_sound_start=next_dma_sound_start;
    dma_sound_end=next_dma_sound_end;
    dma_sound_fetch_address=dma_sound_start; //SS IO registers
  }else if ((dma_sound_control & BIT_0)==0 && (io_src_b & BIT_0)){ //Start playing
    TRACE_LOG("start ");
    dma_sound_start=next_dma_sound_start;
    dma_sound_end=next_dma_sound_end;
    dma_sound_fetch_address=dma_sound_start;
    TRACE_LOG("frame %x-%x ",dma_sound_start,dma_sound_end);
    if (dma_sound_on_this_screen==0){
      // Pad buffer with last byte from VBL to current position
      bool Mono=bool(dma_sound_mode & BIT_7);
      TRACE_LOG((Mono) ? (char*)"Mono":(char*)"Stereo");
      int freq_idx=0;
      if (shifter_freq_at_start_of_vbl==60) freq_idx=1;
      if (shifter_freq_at_start_of_vbl==MONO_HZ) freq_idx=2;
      WORD w1,w2;
      dma_sound_get_last_sample(&w1,&w2);
      for (int y=-scanlines_above_screen[freq_idx];y<scan_y;y++){
        if (Mono){  //play half as many words
          dma_sound_samples_countdown+=dma_sound_freq*scanline_time_in_cpu_cycles_at_start_of_vbl/2;
        }else{ //stereo, 1 word per sample
          dma_sound_samples_countdown+=dma_sound_freq*scanline_time_in_cpu_cycles_at_start_of_vbl;
        }
        int loop=int(Mono ? 2:1);
        while (dma_sound_samples_countdown>=0){
          for (int i=0;i<loop;i++){
            dma_sound_output_countdown+=sound_freq;
            while (dma_sound_output_countdown>=0){
              if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) break;
              dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w1;
              dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w2;
              dma_sound_output_countdown-=dma_sound_freq;
            }
          }
          dma_sound_samples_countdown-=n_cpu_cycles_per_second;
        }
      }
      dma_sound_on_this_screen=1;
    }
  }
#if defined(SSE_SOUND_DMA_LIGHT) //hack for Light megademo screen by New Core
  else if(OPTION_HACKS && (io_src_b&BIT_0) && !(dma_sound_control&BIT_1))
  {
    TRACE_LOG("DMA restart ");
    dma_sound_start=next_dma_sound_start;
    dma_sound_end=next_dma_sound_end;
    dma_sound_fetch_address=dma_sound_start;
  }
#endif
  TRACE_LOG(" Freq %d\n",dma_sound_freq);
  log_to(LOGSECTION_SOUND,EasyStr("SOUND: ")+HEXSl(old_pc,6)+" - DMA sound control set to "+(io_src_b & 3)+" from "+(dma_sound_control & 3));
  dma_sound_control=io_src_b;
#if !defined(SSE_SOUND_DMA) //remove TOS condition
  if (tos_version>=0x106) 
#endif
  mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,bool(COLOUR_MONITOR)^bool(dma_sound_control & BIT_0));
}
//---------------------------------------------------------------------------
void dma_sound_set_mode(BYTE new_mode)
{
/*
  DMA-Soundmode Register:

    $FFFF8920  0 0 0 0 0 0 0 0 X 0 0 0 X X X X

  Allows to toggle between several replay methods. Bit 7 switches Mono/Stereo 
  (1 = Mono, 0 = Stereo), Bit 0 and 1 encode the replay rate:
      0 0  -  6258 Hz
      0 1  - 12517 Hz
      1 0  - 25033 Hz
      1 1  - 50066 Hz
  01 Cool STE, Delirious 4 loaders
  80 RGBeast

  The ST-E's stereo sound allows playback at fixed sampling frequencies only
  (6.25 KHz, 12.5 KHz, 25 KHz and 50 KHz), 
  playing a sample back at different frequencies than it was recorded at 
  needs the CPU to stretch or shrink the sample data
  Amiga was more flexible
  Also, the ST-E's stereo output is purely a 2 channel system, so playing 
  more than 2 samples together (for example, playing a MOD file) requires 
  the CPU to mix the sample information together.
*/

#if defined(SSE_SOUND)
  new_mode&=0x8F;
  TRACE_LOG("DMA sound mode %X freq %d\n",new_mode,dma_sound_mode_to_freq[new_mode & 3]);
#endif

  dma_sound_mode=new_mode;
  dma_sound_freq=dma_sound_mode_to_freq[dma_sound_mode & 3];
#if defined(SSE_SOUND_MICROWIRE_392)
  SampleRate=sound_freq;
#elif defined(SSE_SOUND_MICROWIRE)
  SampleRate=dma_sound_freq; // global of 3rd party dsp
#endif
  log_to(LOGSECTION_SOUND,EasyStr("SOUND: ")+HEXSl(old_pc,6)+" - DMA sound mode set to $"+HEXSl(dma_sound_mode,2)+" freq="+dma_sound_freq);
}
//---------------------------------------------------------------------------
void dma_sound_fetch()
{
/*
  During horizontal blanking (transparent to the processor) the DMA sound
  chip fetches samples from memory and provides them to a digital-to-analog
  converter (DAC) at one of several constant rates, programmable as 
  (approximately) 50KHz (kilohertz), 25KHz, 12.5KHz, and 6.25KHz. 
  This rate is called the sample frequency. 

   Each sample is stored as a signed eight-bit quantity, where -128 (80 hex) 
   means full negative displacement of the speaker, and 127 (7F hex) means full 
   positive displacement. In stereo mode, each word represents two samples: the 
   upper byte is the sample for the left channel, and the lower byte is the 
   sample for the right channel. In mono mode each byte is one sample. However, 
   the samples are always fetched a word at a time, so only an even number of 
   mono samples can be played. 

   This function is called by event_hbl() & event_scanline(), which matches the
   hblank timing. [TODO: when DE ends?]
   It sends some samples to the rendering buffer, then fill up the FIFO buffer.

   A possible problem in Steem (vs. Hatari) is that there's no independent clock
   for DMA sound, it's based on CPU speed.
*/
  bool Playing=bool(dma_sound_control & BIT_0);
  bool Mono=bool(dma_sound_mode & BIT_7);

  //we want to play a/b samples, where a is the DMA sound frequency
  //and b is the number of scanlines a second

  int left_vol_top_val=dma_sound_l_top_val,right_vol_top_val=dma_sound_r_top_val;
  //this a/b is the same as dma_sound_freq*scanline_time_in_cpu_cycles/8million

/*  SS
    About the a/b thing...
    It works this way with the 'hardware test' of ijbk, and if we change it it
    doesn't anymore but:

    At 50hz, scanline_time_in_cpu_cycles_at_start_of_vbl = 512
    #scanlines = 313x50 = 15650

    freq/15650 = freq*512/8000000 ?

    Then we would have: 8000000/15650 = 512
    Yet 8000000/512 = 15625

    It works with a DMA clock = 8012800

*/

  if (Mono){  //play half as many words
    dma_sound_samples_countdown+=dma_sound_freq*scanline_time_in_cpu_cycles_at_start_of_vbl/2;
    left_vol_top_val=(dma_sound_l_top_val >> 1)+(dma_sound_r_top_val >> 1);
    right_vol_top_val=left_vol_top_val;
  }else{ //stereo, 1 word per sample
    dma_sound_samples_countdown+=dma_sound_freq*scanline_time_in_cpu_cycles_at_start_of_vbl;
  }
  bool vol_change_l=(left_vol_top_val<128),vol_change_r=(right_vol_top_val<128);

  while (dma_sound_samples_countdown>=0){
    //play word from buffer
    if (dma_sound_internal_buf_len>0){
      dma_sound_last_word=dma_sound_internal_buf[0];
      for (int i=0;i<3;i++) dma_sound_internal_buf[i]=dma_sound_internal_buf[i+1];
      dma_sound_internal_buf_len--;
      if (vol_change_l){
       // debug1=dma_sound_last_word;
        int b1=(signed char)(HIBYTE(dma_sound_last_word));
        b1*=left_vol_top_val;
        b1/=128;
        dma_sound_last_word&=0x00ff;
        dma_sound_last_word|=WORD(BYTE(b1) << 8);
        //TRACE_OSD("%x %x",debug1,dma_sound_last_word);
      }
      if (vol_change_r){
        int b2=(signed char)(LOBYTE(dma_sound_last_word));
        b2*=right_vol_top_val;
        b2/=128;
        dma_sound_last_word&=0xff00;
        dma_sound_last_word|=BYTE(b2);
      }
#if defined(SSE_SOUND_16BIT_CENTRED)
      if(!RENDER_SIGNED_SAMPLES)
#endif
      dma_sound_last_word^=WORD((128 << 8) | 128); // unsign
    }
    dma_sound_output_countdown+=sound_freq;
    WORD w1;
    WORD w2;

    if (Mono){       //mono, play half as many words
#if defined(SSE_SOUND_16BIT_CENTRED)
      if (RENDER_SIGNED_SAMPLES)
      {
        w1=WORD((dma_sound_last_word & 0xff00)>>8);
        w2=WORD((dma_sound_last_word & 0x00ff));
      }
      else
      {
        w1=WORD((dma_sound_last_word & 0xff00) >> 2);
        w2=WORD((dma_sound_last_word & 0x00ff) << 6);
      }
#else
      w1=WORD((dma_sound_last_word & 0xff00) >> 2);
      w2=WORD((dma_sound_last_word & 0x00ff) << 6);
#endif
      // dma_sound_channel_buf always stereo, so put each mono sample in twice
      while (dma_sound_output_countdown>=0){
        if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) break;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w1;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w1;
        dma_sound_output_countdown-=dma_sound_freq;
      }
      dma_sound_output_countdown+=sound_freq;
      while (dma_sound_output_countdown>=0){
        if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) break;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w2;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w2;
        dma_sound_output_countdown-=dma_sound_freq;
      }
    }else{//stereo , 1 word per sample
/*
 Stereo Samples have to be organized wordwise like 
 Lowbyte -> right channel
 Hibyte  -> left channel
*/
      if (sound_num_channels==1){
        //average the channels out
#if defined(SSE_SOUND_16BIT_CENTRED)
        if (RENDER_SIGNED_SAMPLES)
          w1=WORD(((dma_sound_last_word & 255)+(dma_sound_last_word >> 8))
#ifndef SSE_BUGFIX_394 //nice double shift
          >>8
#endif          
          );
        else
#endif
        w1=WORD(((dma_sound_last_word & 255)+(dma_sound_last_word >> 8)) << 5);
        w2=0; // skipped //SS no mixdown...
#if defined(SSE_SOUND_16BIT_CENTRED)
      }else if (RENDER_SIGNED_SAMPLES){
        w1=WORD((dma_sound_last_word & 0xff00)>>8);
        w2=WORD((dma_sound_last_word & 0x00ff));
#endif
      }else{
        w1=WORD((dma_sound_last_word & 0xff00) >> 2);
        w2=WORD((dma_sound_last_word & 0x00ff) << 6);
      }
      
      while (dma_sound_output_countdown>=0){
        if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) break;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w1;
        dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=w2;
        dma_sound_output_countdown-=dma_sound_freq;
      }
    }
    dma_sound_samples_countdown-=n_cpu_cycles_per_second; 
  }//while (dma_sound_samples_countdown>=0)

  if (Playing==0) return;
  if (dma_sound_internal_buf_len>=4) return;
  if (dma_sound_fetch_address>=himem) return;

  //SS fill FIFO with up to 8 bytes

  for (int i=0;i<4;i++){
#if defined(SSE_SOUND_DMA_INSANE)
/*  If by any chance a DMA sound is started with frame end = frame start,
    the STE won't stop the sound at once.
    Funny bug in A Little Bit Insane by Lazer.
*/
    if (dma_sound_fetch_address==dma_sound_end){
#else
    if (dma_sound_fetch_address>=dma_sound_end){
#endif
/*
       SS reset loop - immediate?
 A group of samples is called a "frame." A frame may be played once or can 
 automatically be repeated forever (until stopped). A frame is described by 
 its start and end addresses. The end address of a frame is actually the 
 address of the first byte in memory beyond the frame; a frame starting at 
 address 21100 which is 10 bytes long has an end address of 21110. 
 -> this seems OK in Steem
*/

      ////TRACE_LOG("DMA sound reset loop %X->%X\n",next_dma_sound_start,next_dma_sound_end);
      //TRACE_LOG("%d DMA frame end reached (loop %d)\n",ABSOLUTE_CPU_TIME,!!(dma_sound_control & BIT_1));

      dma_sound_start=next_dma_sound_start;
      dma_sound_end=next_dma_sound_end;
      dma_sound_fetch_address=dma_sound_start;
      dma_sound_control&=~BIT_0;

      DMA_SOUND_CHECK_TIMER_A
#if !defined(SSE_SOUND_DMA_390E) //remove TOS condition
      if (tos_version>=0x106) 
#endif
      mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,bool(COLOUR_MONITOR)^bool(dma_sound_control & BIT_0));

      if (dma_sound_control & BIT_1){
        dma_sound_control|=BIT_0; //Playing again immediately
#if !defined(SSE_SOUND_DMA_390E) //remove TOS condition
        if (tos_version>=0x106) 
#endif
        mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,bool(COLOUR_MONITOR)^bool(dma_sound_control & BIT_0));
      }else{
        break;
      }
    }
    dma_sound_internal_buf[dma_sound_internal_buf_len++]=DPEEK(dma_sound_fetch_address);
    dma_sound_fetch_address+=2;
    if (dma_sound_internal_buf_len>=4) break;
  }//nxt
}
//---------------------------------------------------------------------------
#if defined(SSE_CARTRIDGE_BAT)
/*  To play the digital sound of the MV16 cartridge, we hack Steem's STE
    dma sound emulation, trading stereo 8bit for mono 16bit.
    This reduces overhead (need no other sound buffer).
    The sound is played as is, without filter. 
    https://www.youtube.com/watch?v=2cYJGTL0QrE actual hardware
    https://www.youtube.com/watch?v=nxAHqYP9hAg crack - PSG
    This also handles the Replay 16 cartridge and the Centronics cartridge
    used by Wings of Death and Lethal Xcess.
*/
void dma_mv16_fetch(WORD data) {
#if defined(SSE_DONGLE_PROSOUND)
  ASSERT(SSEConfig.mv16 || (DONGLE_ID==TDongle::PROSOUND));
#else
  ASSERT(SSEConfig.mv16);
#endif
#define last_write MicroWire_StartTime //recycle an int, starts at 0
#if defined(SSE_CARTRIDGE_REPLAY16)
  if(!SSEConfig.mr16) // real 16bit?
#endif
    data<<=3;
  int cycles=(ACT-last_write)&0xFFF; //fff for when last_write is 0!
  dma_sound_samples_countdown+=dma_sound_freq*cycles; // this implies jitter
  while (dma_sound_samples_countdown>/*=*/0){
    dma_sound_output_countdown+=sound_freq;
    while (dma_sound_output_countdown>=0){
      if (dma_sound_channel_buf_last_write_t>=DMA_SOUND_BUFFER_LENGTH) 
        break;
      dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=dma_sound_last_word;
      //dma_sound_channel_buf[dma_sound_channel_buf_last_write_t++]=(dma_sound_last_word+data)/2;//filter
      dma_sound_output_countdown-=dma_sound_freq;
    }
    dma_sound_output_countdown+=sound_freq;
    dma_sound_samples_countdown-=n_cpu_cycles_per_second; 
  }
  last_write=ACT;
  dma_sound_on_this_screen=true;
  dma_sound_last_word=data;
}

#undef last_write 

#endif

#ifdef VC_BUILD
#pragma warning (default: 4018) //signed/unsigned mismatch
#endif

//---------------------------------------------------------------------------
void dma_sound_get_last_sample(WORD *pw1,WORD *pw2)
{
  if (dma_sound_mode & BIT_7){
    // ST plays HIBYTE, LOBYTE, so last sample is LOBYTE
#if defined(SSE_SOUND_16BIT_CENTRED)
    if (RENDER_SIGNED_SAMPLES)
      *pw1=WORD((dma_sound_last_word & 0x00ff));
    else
#endif
    *pw1=WORD((dma_sound_last_word & 0x00ff) << 6);
    *pw2=*pw1; // play the same in both channels, or ignored in when sound_num_channels==1
  }else{
    if (sound_num_channels==1){
      //average the channels out
#if defined(SSE_SOUND_16BIT_CENTRED)
      if (RENDER_SIGNED_SAMPLES)
        *pw1=WORD(((dma_sound_last_word & 255)+(dma_sound_last_word >> 8))>>8);
      else
#endif
      *pw1=WORD(((dma_sound_last_word & 255)+(dma_sound_last_word >> 8)) << 5);
      *pw2=0; // skipped
    }else{
#if defined(SSE_SOUND_16BIT_CENTRED)
      if (RENDER_SIGNED_SAMPLES)
      {
        *pw1=WORD((dma_sound_last_word & 0xff00)>>8);
        *pw2=WORD((dma_sound_last_word & 0x00ff));
      }
      else
      {
        *pw1=WORD((dma_sound_last_word & 0xff00) >> 2);
        *pw2=WORD((dma_sound_last_word & 0x00ff) << 6);
      }
#else
      *pw1=WORD((dma_sound_last_word & 0xff00) >> 2);
      *pw2=WORD((dma_sound_last_word & 0x00ff) << 6);
#endif
    }
  }
}
//---------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*                                PSG SOUND                                  */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#if !defined(SSE_VAR_DEAD_CODE)
#define PSG_CALC_VOLTAGE_ENVELOPE                                     \
      {																																	\
        envstage=(t64-est64)/envmodulo2;           \
        if (envstage>=32 && envdeath!=-1){                           \
          *(p++)+=envdeath;                                             \
        }else{                                                       \
          *(p++)+=psg_envelope_level[envshape][envstage & 63];            \
        }																															\
      }
#endif

#define PSG_PULSE_NOISE(ntn) (psg_noise[(ntn) MOD_PSG_NOISE_ARRAY] )
#define PSG_PULSE_TONE  ((t*128 / psg_tonemodulo) & 1)
#define PSG_PULSE_TONE_t64  ((t*64 / psg_tonemodulo_2) & 1)


#if defined(SSE_SOUND_INLINE2)
/*  Second round of inlining
    Necessary for SSE_YM2149_DELAY_RENDERING
*/

void psg_prepare_envelope(double &af,double &bf,int &psg_envmodulo,DWORD t,
  int &psg_envstage,int &psg_envcountdown,int &envdeath,int &envshape,int &envvol) {
  int envperiod=max( (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]) <<8) + psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);
  af=envperiod;
  af*=sound_freq;                  
  af*=((double)(1<<13))/15625;                               
  psg_envmodulo=(int)af; 
  bf=(((DWORD)t)-psg_envelope_start_time); 
  bf*=(double)(1<<17); 
  psg_envstage=(int)floor(bf/af); 
  bf=fmod(bf,af); /*remainder*/ 
  psg_envcountdown=psg_envmodulo-(int)bf; 
  envdeath=-1;                                                                  
  if ((psg_reg[PSGR_ENVELOPE_SHAPE] & PSG_ENV_SHAPE_CONT)==0 ||                  
    (psg_reg[PSGR_ENVELOPE_SHAPE] & PSG_ENV_SHAPE_HOLD)){                     
      if(psg_reg[PSGR_ENVELOPE_SHAPE]==11 || psg_reg[PSGR_ENVELOPE_SHAPE]==13){      
#if defined(SSE_YM2149_DELAY_RENDERING)
        envdeath=(OPTION_SAMPLED_YM)?31:psg_flat_volume_level[15];                                           
#else
        envdeath=psg_flat_volume_level[15];                                           
#endif
      }else{       
#if defined(SSE_YM2149_DELAY_RENDERING) 
        envdeath=(OPTION_SAMPLED_YM)?0:psg_flat_volume_level[0];                                       
#else                                                                    
        envdeath=psg_flat_volume_level[0];     
#endif                                         
      }                                                                                   
  }                                                                                      
  envshape=psg_reg[PSGR_ENVELOPE_SHAPE] & 7;                    
  if (psg_envstage>=32 && envdeath!=-1){                           
    envvol=envdeath;                                             
  }else{        
#if defined(SSE_YM2149_DELAY_RENDERING)  
    envvol=(OPTION_SAMPLED_YM) 
      ? psg_envelope_level3[envshape][psg_envstage & 63]
    : psg_envelope_level[envshape][psg_envstage & 63] ;                   
#else
    envvol=psg_envelope_level[envshape][psg_envstage & 63];
#endif
  }																			
}

void psg_prepare_noise(double &af,double &bf,int &psg_noisemodulo,DWORD t,
    int &psg_noisecountdown, int &psg_noisecounter,bool &psg_noisetoggle) {
  int noiseperiod=(1+(psg_reg[PSGR_NOISE_PERIOD]&0x1f)); //TODO 1+...
  af=((int)noiseperiod*sound_freq);                              
  af*=((double)(1<<17))/15625; 
  psg_noisemodulo=(int)af; 
  bf=t;
  bf*=(double)(1<<20); 
  psg_noisecounter=(int)floor(bf/af); 
  psg_noisecounter &= (PSG_NOISE_ARRAY-1); 
  bf=fmod(bf,af); 
  psg_noisecountdown=psg_noisemodulo-(int)bf; 
  psg_noisetoggle=psg_noise[psg_noisecounter];
}

void psg_prepare_tone(int toneperiod,double &af,double &bf,
                      int &psg_tonemodulo_2,int abc,DWORD t,
                      int &psg_tonecountdown,bool &psg_tonetoggle) {
  af=((int)toneperiod*sound_freq);                              
  af*=((double)(1<<17))/15625;                               
  psg_tonemodulo_2=(int)af; 
  bf=(((DWORD)t)-psg_tone_start_time[abc]); 
  bf*=(double)(1<<21); 
  bf=fmod(bf,af*2); 
  af=bf-af;               
  if(af>=0){                  
    psg_tonetoggle=false;       
    bf=af;                      
  }                           
  psg_tonecountdown=psg_tonemodulo_2-(int)bf; 
}


void psg_envelope_advance(int &psg_envmodulo,int &psg_envstage,
         int &psg_envcountdown,int &envdeath,int &envshape,int &envvol) {
  psg_envcountdown-=TWO_TO_SEVENTEEN; //  131072
  while (psg_envcountdown<0){           
    psg_envcountdown+=psg_envmodulo;             
    psg_envstage++;                 
    if (psg_envstage>=32 && envdeath!=-1){                           
      envvol=envdeath;                                             
    }else{   
#if defined(SSE_YM2149_DELAY_RENDERING)  
      envvol=(OPTION_SAMPLED_YM) 
        ? psg_envelope_level3[envshape][psg_envstage & 63]
      : psg_envelope_level[envshape][psg_envstage & 63] ;                   
#else                                                    
      envvol=psg_envelope_level[envshape][psg_envstage & 63];
#endif
    }																															
  }
}

void psg_tone_advance(int psg_tonemodulo_2,int &psg_tonecountdown,
                      bool &psg_tonetoggle) {
  psg_tonecountdown-=TWO_MILLION;  
  while (psg_tonecountdown<0){           
    psg_tonecountdown+=psg_tonemodulo_2;             
    psg_tonetoggle=!psg_tonetoggle;                   
  }
}

void psg_noise_advance(int psg_noisemodulo,int &psg_noisecountdown,int &psg_noisecounter,bool &psg_noisetoggle) {
  psg_noisecountdown-=ONE_MILLION;   
  while (psg_noisecountdown<0){   
    psg_noisecountdown+=psg_noisemodulo;   
    psg_noisecounter++;                        
    if(psg_noisecounter>=PSG_NOISE_ARRAY){      
      psg_noisecounter=0;                        
    }
    psg_noisetoggle=psg_noise[psg_noisecounter];   
  }
}

#define PSG_PREPARE_ENVELOPE psg_prepare_envelope(af,bf,psg_envmodulo,t,psg_envstage,psg_envcountdown,envdeath,envshape,envvol);
#define PSG_PREPARE_TONE psg_prepare_tone(toneperiod,af,bf,psg_tonemodulo_2,abc,t,psg_tonecountdown,psg_tonetoggle);
#define PSG_ENVELOPE_ADVANCE  psg_envelope_advance(psg_envmodulo,psg_envstage,psg_envcountdown,envdeath,envshape,envvol);
#define PSG_TONE_ADVANCE psg_tone_advance(psg_tonemodulo_2,psg_tonecountdown,psg_tonetoggle);
#define PSG_PREPARE_NOISE psg_prepare_noise(af,bf,psg_noisemodulo,t,psg_noisecountdown,psg_noisecounter,psg_noisetoggle);
#define PSG_NOISE_ADVANCE  psg_noise_advance(psg_noisemodulo,psg_noisecountdown,psg_noisecounter,psg_noisetoggle);

#else

#define PSG_PREPARE_ENVELOPE                                \
      int envperiod=max( (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]) <<8) + psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);  \
      af=envperiod;                              \
      af*=sound_freq;                  \
      af*=((double)(1<<13))/15625;                               \
      psg_envmodulo=(int)af; \
      bf=(((DWORD)t)-psg_envelope_start_time); \
      bf*=(double)(1<<17); \
      psg_envstage=(int)floor(bf/af); \
      bf=fmod(bf,af); /*remainder*/ \
      psg_envcountdown=psg_envmodulo-(int)bf; \
      envdeath=-1;                                                                  \
      if ((psg_reg[PSGR_ENVELOPE_SHAPE] & PSG_ENV_SHAPE_CONT)==0 ||                  \
           (psg_reg[PSGR_ENVELOPE_SHAPE] & PSG_ENV_SHAPE_HOLD)){                      \
        if(psg_reg[PSGR_ENVELOPE_SHAPE]==11 || psg_reg[PSGR_ENVELOPE_SHAPE]==13){      \
          envdeath=psg_flat_volume_level[15];                                           \
        }else{                                                                           \
          envdeath=psg_flat_volume_level[0];                                              \
        }                                                                                   \
      }                                                                                      \
      envshape=psg_reg[PSGR_ENVELOPE_SHAPE] & 7;                    \
      if (psg_envstage>=32 && envdeath!=-1){                           \
        envvol=envdeath;                                             \
      }else{                                                       \
        envvol=psg_envelope_level[envshape][psg_envstage & 63];            \
      }																															\


#define PSG_PREPARE_NOISE                                \
      int noiseperiod=(1+(psg_reg[PSGR_NOISE_PERIOD]&0x1f));      \
      af=((int)noiseperiod*sound_freq);                              \
      af*=((double)(1<<17))/15625; \
      psg_noisemodulo=(int)af; \
      bf=t; \
      bf*=(double)(1<<20); \
      psg_noisecounter=(int)floor(bf/af); \
      psg_noisecounter &= (PSG_NOISE_ARRAY-1); \
      bf=fmod(bf,af); \
      psg_noisecountdown=psg_noisemodulo-(int)bf; \
      psg_noisetoggle=psg_noise[psg_noisecounter];

      /*
      if (abc==0) log_write(Str("toneperiod=")+toneperiod+" sound_freq="+sound_freq+" psg_tonemodulo_2="+psg_tonemodulo_2); \
      */


#define PSG_PREPARE_TONE                                 \
      af=((int)toneperiod*sound_freq);                              \
      af*=((double)(1<<17))/15625;                               \
      psg_tonemodulo_2=(int)af; \
      bf=(((DWORD)t)-psg_tone_start_time[abc]); \
      bf*=(double)(1<<21); \
      bf=fmod(bf,af*2); \
      af=bf-af;               \
      if(af>=0){                  \
        psg_tonetoggle=false;       \
        bf=af;                      \
      }                           \
      psg_tonecountdown=psg_tonemodulo_2-(int)bf; \


#define PSG_ENVELOPE_ADVANCE                                   \
          psg_envcountdown-=TWO_TO_SEVENTEEN;  \
          while (psg_envcountdown<0){           \
            psg_envcountdown+=psg_envmodulo;             \
            psg_envstage++;                   \
            if (psg_envstage>=32 && envdeath!=-1){                           \
              envvol=envdeath;                                             \
            }else{                                                       \
              envvol=psg_envelope_level[envshape][psg_envstage & 63];            \
            }																															\
          }
  //            envvol=(psg_envstage&255)*64;


#define PSG_TONE_ADVANCE                                   \
          psg_tonecountdown-=TWO_MILLION;  \
          while (psg_tonecountdown<0){           \
            psg_tonecountdown+=psg_tonemodulo_2;             \
            psg_tonetoggle=!psg_tonetoggle;                   \
          }


#define PSG_NOISE_ADVANCE                           \
          psg_noisecountdown-=ONE_MILLION;   \
          while (psg_noisecountdown<0){   \
            psg_noisecountdown+=psg_noisemodulo;      \
            psg_noisecounter++;                        \
            if(psg_noisecounter>=PSG_NOISE_ARRAY){      \
              psg_noisecounter=0;                        \
            }                                             \
            psg_noisetoggle=psg_noise[psg_noisecounter];   \
          }


#endif

/*  SS:This function renders one PSG channel until timing to_t.
    v3.7.0, when option 'P.S.G.' is checked, rendering is delayed
    until VBL time so that the unique table for all channels may be
    used instead. Digital volume is written, as well as on/off info
    and fixed/envelope.
*/

void psg_write_buffer(int abc,DWORD to_t)
{
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
  // It was a request and I received no thanks no feedback from the amiga lamer
  if( (4>>abc) & (d2_dpeek(FAKE_IO_START+20)>>12 ))
    return; // skip this channel
#endif
#if defined(SSE_CARTRIDGE_BAT2)
/*  B.A.T II plays the same samples on both the MV16 and the PSG, because
    the MV16 of B.A.T I wasn't included, it was just supported.
    If player inserted the cartridge, he doesn't need PSG sounds.
*/
  if(SSEConfig.mv16 && DONGLE_ID==TDongle::BAT2)
    return; 
#endif
  //buffer starts at time time_of_last_vbl
  //we've written up to psg_buf_pointer[abc]
  //so start at pointer and write to to_t,

  int psg_tonemodulo_2,psg_noisemodulo;
  int psg_tonecountdown,psg_noisecountdown;
  int psg_noisecounter;
  double af,bf;
  bool psg_tonetoggle=true,psg_noisetoggle;
  int *p=psg_channels_buf+psg_buf_pointer[abc];
  ASSERT(p-psg_channels_buf<=PSG_CHANNEL_BUF_LENGTH);
  DWORD t=(psg_time_of_last_vbl_for_writing+psg_buf_pointer[abc]);//SS where we are now
  to_t=max(to_t,t);//SS can't go backwards
  to_t=min(to_t,psg_time_of_last_vbl_for_writing+PSG_CHANNEL_BUF_LENGTH);//SS don't exceed buffer
  int count=max(min((int)(to_t-t),PSG_CHANNEL_BUF_LENGTH-psg_buf_pointer[abc]),0);//SS don't exceed buffer
  ASSERT( count>=0 );
  TRACE_PSG("write %d samples\n",count);
#if defined(SSE_SOUND_OPT1)
  if(!count)
    return;
#endif
  int toneperiod=(((int)psg_reg[abc*2+1] & 0xf) << 8) + psg_reg[abc*2];

  TRACE_PSG("Write buffer %d from %d to %d\n",abc,t,to_t);

  if ((psg_reg[abc+8] & BIT_4)==0){ // Not Enveloped //SS bit 'M' in those registers
    int vol=psg_flat_volume_level[psg_reg[abc+8] & 15];
    if ((psg_reg[PSGR_MIXER] & (1 << abc))==0 && (toneperiod>9)){ //tone enabled
      PSG_PREPARE_TONE
      if (
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS_NOISE)
        !((1<<11)&d2_dpeek(FAKE_IO_START+20)) &&
#endif
        (psg_reg[PSGR_MIXER] & (8 << abc))==0){ //noise enabled

        PSG_PREPARE_NOISE
        for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
/*  We don't render (compute volume) here, we record digital volume
    instead if option 'P.S.G.' is checked
    Volume is coded on 5 bit (fixed volume is shifted)
*/
          if(OPTION_SAMPLED_YM)
          {
            int t=0;
            if(psg_tonetoggle || psg_noisetoggle)
              ; // mimic Steem's way
            else
              t|=(psg_reg[abc+8] & 15)<<1; // vol 4bit
            t<<=8*abc;
            *p|=t;
            p++;
          }else
#endif // Steem 3.2's immediate rendering
          if(psg_tonetoggle || psg_noisetoggle){
            p++;
          }else{
            *(p++)+=vol;
          }
          ASSERT(p-psg_channels_buf<=PSG_CHANNEL_BUF_LENGTH);
          PSG_TONE_ADVANCE
          PSG_NOISE_ADVANCE
        }
      }else{ //tone only
        for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
          if(OPTION_SAMPLED_YM)
          {
            int t=0;
            if(psg_tonetoggle)
              ;
            else
              t|=(psg_reg[abc+8] & 15)<<1; // vol 4bit
            t<<=8*abc;
            *p|=t;
            p++;
          } else
#endif
          if(psg_tonetoggle){
            p++;
          }else{
            *(p++)+=vol;
          }
          ASSERT(p-psg_channels_buf<=PSG_CHANNEL_BUF_LENGTH);
          PSG_TONE_ADVANCE
        }
      }
    }else if (
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS_NOISE)
      !((1<<11)&d2_dpeek(FAKE_IO_START+20)) &&
#endif      
      (psg_reg[PSGR_MIXER] & (8 << abc))==0){ //noise enabled
      PSG_PREPARE_NOISE
      for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
        if(OPTION_SAMPLED_YM)
        {
          int t=0;
          if(psg_noisetoggle)
            ;
          else
            t|=(psg_reg[abc+8] & 15)<<1; // vol 4bit
          t<<=8*abc;
          *p|=t;
          p++;
        } else
#endif
        if(psg_noisetoggle){
          p++;
        }else{
          *(p++)+=vol;
        }
        ASSERT(p-psg_channels_buf<=PSG_CHANNEL_BUF_LENGTH);
        PSG_NOISE_ADVANCE
      }
    }else{ //nothing enabled //SS playing samples
      for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
        if(OPTION_SAMPLED_YM)
        {
          int t=0;
          t|=(psg_reg[abc+8] & 15)<<1; // vol 4bit
          t<<=8*abc;
          *p|=t;
          p++;
        } else
#endif
        *(p++)+=vol;
        ASSERT(p-psg_channels_buf<=PSG_CHANNEL_BUF_LENGTH);
      }
    }
    psg_buf_pointer[abc]=to_t-psg_time_of_last_vbl_for_writing;
    return;
  }else
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS_ENV)
    if(!((1<<10)&d2_dpeek(FAKE_IO_START+20))) //'mute env'
#endif
  {  
 
   // Enveloped

//    DWORD est64=psg_envelope_start_time*64;
    int envdeath,psg_envstage,envshape;
    int psg_envmodulo,envvol,psg_envcountdown;
    PSG_PREPARE_ENVELOPE;
// double &af,double &bf,int &psg_envmodulo,DWORD t,
//  int &psg_envstage,int &psg_envcountdown,int &envdeath,int &envshape,int &envvol
    if ((psg_reg[PSGR_MIXER] & (1 << abc))==0 && (toneperiod>9)){ //tone enabled
      PSG_PREPARE_TONE
      if (
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS_NOISE)
        !((1<<11)&d2_dpeek(FAKE_IO_START+20)) &&
#endif
        (psg_reg[PSGR_MIXER] & (8 << abc))==0){ //noise enabled
        PSG_PREPARE_NOISE
        for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
          if(OPTION_SAMPLED_YM)
          {
            int t=BIT_6; // code for enveloped
            if(psg_tonetoggle || psg_noisetoggle)
              ;
            else
              t|=envvol; // vol 5bit - check 'prepare'
            t<<=8*abc;
            *p|=t;
            p++;
          } else
#endif
          if(psg_tonetoggle || psg_noisetoggle){
            p++;
          }else{
            *(p++)+=envvol;
          }
          PSG_TONE_ADVANCE
          PSG_NOISE_ADVANCE
          PSG_ENVELOPE_ADVANCE
        }
      }else{ //tone only
        for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
          if(OPTION_SAMPLED_YM)
          {
            int t=BIT_6;
            if(psg_tonetoggle)
              ;
            else
              t|=envvol; // vol 5bit
            t<<=8*abc;
            *p|=t;
            p++;
          } else
#endif
          if(psg_tonetoggle){
            p++;
          }else{
            *(p++)+=envvol;
          }
          PSG_TONE_ADVANCE
          PSG_ENVELOPE_ADVANCE
        }
      }
    }else if (
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS_NOISE)
        !((1<<11)&d2_dpeek(FAKE_IO_START+20)) &&
#endif
      (psg_reg[PSGR_MIXER] & (8 << abc))==0){ //noise enabled
      PSG_PREPARE_NOISE
      for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
        if(OPTION_SAMPLED_YM)
        {
          int t=BIT_6;
          if(psg_noisetoggle)
            ;
          else
            t|=envvol; // vol 5bit
          t<<=8*abc;
          *p|=t;
          p++;
        } else
#endif
        if(psg_noisetoggle){
          p++;
        }else{
          *(p++)+=envvol;
        }
        PSG_NOISE_ADVANCE
        PSG_ENVELOPE_ADVANCE
      }
    }else{ //nothing enabled
      for (;count>0;count--){
#if defined(SSE_YM2149_DELAY_RENDERING)
        if(OPTION_SAMPLED_YM)
        {
          int t=BIT_6; 
          t|=envvol; // vol 5bit
          t<<=8*abc;
          *p|=t;
          p++;
        } else
#endif
        *(p++)+=envvol;
        PSG_ENVELOPE_ADVANCE
      }
    }
    psg_buf_pointer[abc]=to_t-psg_time_of_last_vbl_for_writing;
  }
}

//---------------------------------------------------------------------------
#if !defined(SSE_YM2149_QUANTIZE_382)
DWORD psg_quantize_time(int abc,DWORD t)
{
  int toneperiod=(((int)psg_reg[abc*2+1] & 0xf) << 8) + psg_reg[abc*2];
 if (toneperiod<=1) return t;

  double a,b;
  a=toneperiod*sound_freq;
  a/=(15625*8); //125000
  b=(t-psg_tone_start_time[abc]);
//		b=a-fmod(b,a);
  a=fmod(b,a);
  b-=a;
  ASSERT(t>=psg_tone_start_time[abc]+DWORD(b)); //should be at least t??
  t=psg_tone_start_time[abc]+DWORD(b);
  TRACE_PSG("new t %d\n",t);
  return t;
}
#endif
DWORD psg_adjust_envelope_start_time(DWORD t,DWORD new_envperiod)
{
  double b,c;
  int envperiod=max( (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]) <<8) + psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);

//  a=envperiod;
//  a*=sound_freq;

  b=(t-psg_envelope_start_time);
  c=b*(double)new_envperiod;
  c/=(double)envperiod;
  c=t-c;         //new env start time

//  a/=7812.5; //that's 2000000/256
//  b+=a;
//  a=fmod(b,a);
//  b-=a;
//  t=psg_envelope_start_time+DWORD(b);


  return DWORD(c);
}

/*  SS: this is the function called by iow.cpp when program changes the PSG 
    registers.
    It takes care of writing the appropriate part of the VBL sound buffer
    before the register change.
    If option PSG is checked, it will write the digital volume values, 
    rendering is delayed until VBL to take advantage of the 3 ways volume table.
    old_val has just been read in the PSG register by iow.cpp
    new_val will be put into the register after this function has executed
*/

void psg_set_reg(int reg,BYTE old_val,BYTE &new_val)
{
  ASSERT(reg<=15);

#if defined(SSE_BOILER) //temp
  //FrameEvents.Add(scan_y,LINECYCLES,"YM",reg);
#endif

  // suggestions for global variables:  n_samples_per_vbl=sound_freq/shifter_freq,   shifter_y+(SCANLINES_ABOVE_SCREEN+SCANLINES_BELOW_SCREEN)
  if (reg==1 || reg==3 || reg==5 || reg==13){
    new_val&=15;
  }else if (reg==6 || (reg>=8 && reg<=10)){
    new_val&=31;
  }
  if (reg>=PSGR_PORT_A) return; //SS 14,15

  if (old_val==new_val && reg!=PSGR_ENVELOPE_SHAPE) return;
#if !defined(SSE_YM2149_DISABLE_CAPTURE_FILE)
  ASSERT(!psg_capture_file); // should disable code
  if (psg_capture_file){
    psg_capture_check_boundary();
    DWORD cycle=int(ABSOLUTE_CPU_TIME-psg_capture_cycle_base);
    if (n_millions_cycles_per_sec!=8){
      cycle*=8; // this is safe, max 128000000*8
      cycle/=n_millions_cycles_per_sec;
    }
    BYTE reg_byte=BYTE(reg);

//    log_write(Str("--- cycle=")+cycle+" - reg="+reg_byte+" - val="+new_val);
    fwrite(&cycle,1,sizeof(cycle),psg_capture_file);
    fwrite(&reg_byte,1,sizeof(reg_byte),psg_capture_file);
    fwrite(&new_val,1,sizeof(new_val),psg_capture_file);
  }
#endif
  if (SoundActive()==0){
    dbg_log(Str("SOUND: ")+HEXSl(old_pc,6)+" - PSG reg "+reg+" changed to "+new_val+" at "+scanline_cycle_log());
    return;
  }

#ifdef SSE_BUGFIX_393
  int cpu_cycles_per_vbl=n_cpu_cycles_per_second/shifter_freq_at_start_of_vbl;
#else
  int cpu_cycles_per_vbl=n_cpu_cycles_per_second/shifter_freq; //160000 at 50hz
#endif

#if SCREENS_PER_SOUND_VBL != 1
  cpu_cycles_per_vbl*=SCREENS_PER_SOUND_VBL;
  DWORDLONG a64=(ABSOLUTE_CPU_TIME-cpu_time_of_last_sound_vbl);
#else
  DWORDLONG a64=(ABSOLUTE_CPU_TIME-cpu_time_of_last_vbl);
#endif

  a64*=psg_n_samples_this_vbl; //SS eg *882  (44100/50)
  a64/=cpu_cycles_per_vbl; //SS eg 160420

  DWORD t=psg_time_of_last_vbl_for_writing+(DWORD)a64; //SS t's unit is #samples (total)

  dbg_log(EasyStr("SOUND: PSG reg ")+reg+" changed to "+new_val+" at "+scanline_cycle_log()+"; samples "+t+"; vbl was at "+psg_time_of_last_vbl_for_writing);

  TRACE_PSG("PSG set reg %d = $%X (was %X), t=%d\n",reg,new_val,old_val,t);
#if defined(SSE_YM2149_MAMELIKE)
  if(OPTION_MAME_YM) 
  {
    YM2149.psg_write_buffer(t);
    if(reg==PSGR_ENVELOPE_SHAPE) //note reg still has old value
    {
      ASSERT(YM2149.m_env_step_mask==TYM2149::ENVELOPE_MASK); //32 steps
      YM2149.m_attack = (new_val & 0x04) ? YM2149.m_env_step_mask : 0x00;
      if ((new_val & 0x08) == 0)
      {
        /* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
        YM2149.m_hold = 1;
        YM2149.m_alternate = YM2149.m_attack;
      }
      else
      {
        YM2149.m_hold = new_val & 0x01;
        YM2149.m_alternate = new_val & 0x02;
      }
      YM2149.m_env_step = YM2149.m_env_step_mask;
      YM2149.m_holding = 0;
      ASSERT(YM2149.m_env_step>=0);
      YM2149.m_env_volume = (YM2149.m_env_step ^ YM2149.m_attack);//no need?
// from Hatari, see
// http://www.atari-forum.com/viewtopic.php?f=51&t=31610&sid=b4e1d8fc35b06785db51e4f8e44d013d&p=319952#p319936
      YM2149.m_count_env = 0; 
#if defined(SSE_YM2149_RECORD_YM)
      written_to_env_this_vbl=true;
#endif
    }
  }
  else
#endif
  switch (reg){
    case 0:case 1:
    case 2:case 3:
    case 4:case 5:
    {

      int abc=reg/2;
      // Freq is double bufferred, it cannot change until the PSG reaches the end of the current square wave.
      // psg_tone_start_time[abc] is set to the last end of wave, so if it is in future don't do anything.
      // Overflow will be a problem, however at 50Khz that will take a day of non-stop output.
      if (t>psg_tone_start_time[abc]){
#if defined(SSE_YM2149_QUANTIZE_382)
/*  When the new period is long enough compared with progress of 
    current period, we wait until current wave finishes.
    Otherwise, we start the new wave at once.
    It's a better fix (no hack needed) for high pitch songs in YMT-Player.
    This is based on pym2149 (http://ym2149.org/), or at least on how I read it.
    We need register info, so we don't call psg_quantize_time() anymore,
    we do the computing here.

update, from MAME:
Careful studies of the chip output prove that the chip counts up from 0
until the counter becomes greater or equal to the period. This is an
important difference when the program is rapidly changing the period to
modulate the sound. This is worthwhile noting, since the datasheets
say, that the chip counts down.
*/
        // before change
        int toneperiod1=(((int)psg_reg[abc*2+1] & 0xf) << 8) + psg_reg[abc*2];
        if(toneperiod1>1)
        {
          double a=toneperiod1*sound_freq;
          a/=(15625*8);
          double b=(t-psg_tone_start_time[abc]);
          double a2=fmod(b,a);
          b-=a2;
          DWORD t2=psg_tone_start_time[abc]+DWORD(b); // adjusted start
          // after change
          int toneperiod2 = (reg&1)
            ? (((int)new_val & 0xf) << 8) + psg_reg[abc*2]
            : (((int)psg_reg[abc*2+1] & 0xf) << 8) + new_val;
          double a_bis=toneperiod2*sound_freq;
          a_bis/=(15625*8); 
          // check progress of current wave
          if(!a2 || a2>=a_bis)
            t2=t-a_bis; // start at once
          //TRACE_PSG("Quantize start %d adjusted %d a %f a' %f\n",psg_tone_start_time[abc],t2,a,a_bis);
          //if(t2!=t) TRACE("t %d t2 %d\n",t,t2);
          TRACE_PSG("quantize t %d->%d (%d)\n",t,t2,t2-t);
          t=t2;
        }

#else
        t=psg_quantize_time(abc,t);
#endif//SSE_YM2149_QUANTIZE_382
        psg_write_buffer(abc,t);
        psg_tone_start_time[abc]=t;
        
      }
      break;
    }
    case 6:  //changed noise
      psg_write_buffer(0,t);
      psg_write_buffer(1,t);
      psg_write_buffer(2,t);
      break;
    case 7:  //mixer
//      new_val|=b00111110;
      psg_write_buffer(0,t);
      psg_write_buffer(1,t);
      psg_write_buffer(2,t);
      break;
    case 8:case 9:case 10:  //channel A,B,C volume
//      new_val&=0xf;

      // ST doesn't quantize, it changes the level straight away.
      //  t=psg_quantize_time(reg-8,t);
#if defined(SSE_YM2149_FIXED_VOL_TABLE)
/*  The fixed volume being chosen for all channels at once in case of sample
    playing, we render them before so that volume values are correct at
    each time.
    We could render just once instead, but then we should update pointers TODO
    When user tries to mute PSG channels, we give up this rendering system
    because it's all or nothing.
*/
      if(OPTION_SAMPLED_YM
#if defined(SSE_BOILER_MUTE_SOUNDCHANNELS)
        &&! ((d2_dpeek(FAKE_IO_START+20)>>12)&7)
#endif
        )
      {
        psg_write_buffer(0,t);
        psg_write_buffer(1,t);
        psg_write_buffer(2,t);
      }
      else
#endif
      psg_write_buffer(reg-8,t);

//        psg_tone_start_time[reg-8]=t;
      break;
    case 11: //changing envelope period low
    {
      psg_write_buffer(0,t);
      psg_write_buffer(1,t);
      psg_write_buffer(2,t);
      int new_envperiod=max( (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]) <<8) + new_val,1);
      psg_envelope_start_time=psg_adjust_envelope_start_time(t,new_envperiod);
      break;
    }
    case 12: //changing envelope period high
    {
      psg_write_buffer(0,t);
      psg_write_buffer(1,t);
      psg_write_buffer(2,t);
      int new_envperiod=max( (((int)new_val) <<8) + psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);
      psg_envelope_start_time=psg_adjust_envelope_start_time(t,new_envperiod);
      break;
    }
    case 13: //envelope shape
    {
      //SS commented-out code was that way in released source
/*
      DWORD abc_t[3]={t,t,t};
      for (int abc=0;abc<3;abc++){
        if (psg_reg[8+abc] & 16) abc_t[abc]=psg_quantize_time(abc,t);
      }
*/
//        t=psg_quantize_envelope_time(t,0,&new_envelope_start_time);

      {
      psg_write_buffer(0,t);
      psg_write_buffer(1,t);
      psg_write_buffer(2,t);
      }
      psg_envelope_start_time=t;
/*
      for (int abc=0;abc<3;abc++){
        if (psg_reg[8+abc] & 16) psg_tone_start_time[abc]=abc_t[abc];
      }
*/
#if defined(SSE_YM2149_RECORD_YM)
      written_to_env_this_vbl=true;
#endif
      break;
    }
  }
}

#if !defined(SSE_YM2149_DISABLE_CAPTURE_FILE)

void psg_capture(bool start,Str file)
{
  if (psg_capture_file){
    fclose(psg_capture_file);
    psg_capture_file=NULL;
  }
  if (start){
    psg_capture_file=fopen(file,"wb");
    if (psg_capture_file){
      WORD magic=0x2149;
      DWORD data_start=sizeof(WORD)+sizeof(DWORD)+sizeof(WORD)+sizeof(BYTE)*14;
      WORD version=1;

      fwrite(&magic,1,sizeof(magic),psg_capture_file);
      fwrite(&data_start,1,sizeof(data_start),psg_capture_file);
      fwrite(&version,1,sizeof(version),psg_capture_file);
      fwrite(psg_reg,14,sizeof(BYTE),psg_capture_file);

      psg_capture_cycle_base=ABSOLUTE_CPU_TIME;
    }
  }
}

void psg_capture_check_boundary()
{
  if (int(ABSOLUTE_CPU_TIME-psg_capture_cycle_base)>=(int)n_cpu_cycles_per_second){
    psg_capture_cycle_base+=n_cpu_cycles_per_second;

//    log_write(Str("--- second boundary"));
    DWORD cycle=0;
    BYTE reg_byte=0xff;
    BYTE new_val=0xff;
    fwrite(&cycle,1,sizeof(cycle),psg_capture_file);
    fwrite(&reg_byte,1,sizeof(reg_byte),psg_capture_file);
    fwrite(&new_val,1,sizeof(new_val),psg_capture_file);
  }
}

#endif

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#undef LOGSECTION

