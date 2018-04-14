/*
 * 
 *      Copyright (C) 2012 Edgar Hucek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
//*********************************************************************************
//
// ((PART IF THE CODE FROM THIS FILE IS CURRENTLY IN omxplayerB.cpp FOR MANAGEABILITY))
//
// INCLUDES MAIN() LOOP AND CODE TO HANDLE COMMAND LINE ARGUMENTS.
//
// CREATES THE MASTER OMXClock, OMXReader, AND OMXControl OBJECTS.
//
// CREATES OMXPlayerVideo, OMXPlayerAudio, AND OMXPlayerSubtitles CLASSES WHICH 
// ARE WRAPPERS FOR COMXVideo, COMXAudio, and COMXSubtitles CLASSES THAT OPERATE
// THE CHAIN OF OMX COMPONENTS FOR RENDERING.
//
//*********************************************************************************


#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>

#define AV_NOWARN_DEPRECATED

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "DllAvUtil.h"
#include "DllAvFormat.h"
#include "DllAvCodec.h"
#include "linux/RBP.h"

#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
#include "OMXControl.h"
#include "DllOMX.h"
#include "Srt.h"
#include "KeyConfig.h"
#include "utils/Strprintf.h"
#include "Keyboard.h"

#include <string>
#include <utility>

#include "version.h"

// TRICKPLAY DEFINED HERE**
// TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/REWIND/FAST FORWARD (NOT NORMAL PLAY)
// when we repeatedly seek, rather than play continuously
#define TRICKPLAY(speed) (speed < 0 || speed > 4 * DVD_PLAYSPEED_NORMAL)

#define DISPLAY_TEXT(text, ms) if(m_osd) m_player_subtitles.DisplayText(text, ms)

#define DISPLAY_TEXT_SHORT(text) DISPLAY_TEXT(text, 1000)
#define DISPLAY_TEXT_LONG(text) DISPLAY_TEXT(text, 2000)

#define OFFSCREEN_X1 0    // DEFAULT -- SHOW PRELOADING VIDEO OFF SCREEN
#define OFFSCREEN_Y1 4000
#define OFFSCREEN_X2 100
#define OFFSCREEN_Y2 4100
//*/

/*#define OFFSCREEN_X1 1300 // DEBUG -- SHOW PRELOADING VIDEO IN CORNER OF SCREEN
#define OFFSCREEN_Y1 1000
#define OFFSCREEN_X2 1600
#define OFFSCREEN_Y2 1200
//*/

/*#define OFFSCREEN_X1 0 // DEBUG -- SHOW PRELOADING VIDEO IN CORNER OF SCREEN
#define OFFSCREEN_Y1 0
#define OFFSCREEN_X2 300
#define OFFSCREEN_Y2 300
//*/

// ----------------------------------------------------------------------------------------------
// GLOBAL VARIABLES 

typedef enum {CONF_FLAGS_FORMAT_NONE, CONF_FLAGS_FORMAT_SBS, CONF_FLAGS_FORMAT_TB, CONF_FLAGS_FORMAT_FP } FORMAT_3D_T;
enum PCMChannels  *m_pChannelMap        = NULL;
volatile sig_atomic_t g_abort           = false;
long              m_Volume              = 0;
long              m_Amplification       = 0;
bool              m_NativeDeinterlace   = false;
bool              m_HWDecode            = false;
bool              m_osd                 = true;
bool              m_no_keys             = false;
std::string       m_external_subtitles_path;
bool              m_has_external_subtitles = false;
std::string       m_font_path           = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
std::string       m_italic_font_path    = "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf";
std::string       m_dbus_name           = "org.mpris.MediaPlayer2.omxplayer";
bool              m_asked_for_font      = false;
bool              m_asked_for_italic_font = false;
float             m_font_size           = 0.055f;
bool              m_centered            = false;
bool              m_ghost_box           = true;
unsigned int      m_subtitle_lines      = 3;
bool              m_Pause               = false;
OMXReader         m_omx_reader;
int               m_audio_index_use     = 0;
OMXClock          *m_av_clock           = NULL;
OMXControl        m_omxcontrol;
Keyboard          *m_keyboard           = NULL;
OMXAudioConfig    m_config_audio;
OMXVideoConfig    m_config_video;
OMXPacket         *m_omx_pkt            = NULL;
bool              m_no_hdmi_clock_sync  = false;
bool              m_stop                = false;
int               m_subtitle_index      = -1;
DllBcmHost        m_BcmHost;
OMXPlayerVideo    m_player_video;
OMXPlayerAudio    m_player_audio;
OMXPlayerSubtitles  m_player_subtitles;
int               m_tv_show_info        = 0;
bool              m_has_video           = false;
bool              m_has_audio           = false;
bool              m_has_subtitle        = false;
bool              m_gen_log             = false;
bool              m_loop                = false;
bool              m_seamless_loop       = false; // NEW!!!!!!!!! SLOCUM

// ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - 
// ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - 
// MY OWN GLOBAL VARIABLES - SLOCUM
#include "../mpUIWindowFrame.h"
#include "../mpDebugInclude.h"
std::deque<long long> timeOffsetArray;
double mediaTimeUSec = 0.0;
long long mediaTimeOffsetMSec = 0;
long long videoLength = 0;
long long playlistStartTimeMSec = 0;
bool syncEnabled = false;
long loopCount = 0; // used in OMXReader for seamless looping, should not be used in main loop
long localLoopCount = 0; // this may be offset from the other loopCount
CRect storedVideo;
VideoWallConfig videoWallConfig;
bool videoPositionChanged = false;
int sleepDelayMsec = 1;
long cycleCount = 0; 
long long pauseTimeMSec = 0;
bool startPause = false;
bool startUnpause = false;
enum TimelineStateEnum { 
  STATE_STARTUP, 
  STATE_PRELOADING, 
  STATE_PRELOADING_PAUSED, 
  STATE_PLAYING, 
  STATE_FINISHED_PLAYING, 
  //STATE_SCHEDULING_DISABLED, 
  STATE_PRELOAD_SEEK, 
  //STATE_PRELOAD_SEEK_PAUSE,
  STATE_PRELOADING_PAUSED_FROM_ZERO,
  STATE_USER_PAUSING,
  STATE_USER_PAUSED,
  STATE_USER_UNPAUSING
};
TimelineStateEnum timelineState = STATE_STARTUP;
// ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - 
// ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - ~ - 


enum{ERROR=-1,SUCCESS,ONEBYTE};


//==========================================================================
// GET MILLISECONDS - SHOULD BE GLOBALLY USED SO THAT 
//   I CAN CHANGE THE TIME ACQUISITION METHOD IF NEEDED
long long getCurrentTimeMSec()
{
	// GOOD VERSION...
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long long mslong = (long long) tp.tv_sec * 1000L + tp.tv_usec / 1000; //get current timestamp in milliseconds

	return( (long long) mslong );
}



//-- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- -  -- - -- - -- - -- - -- - -- - -- - -- - 
//#########################################################################################################################################
//#########################################################################################################################################
#include "omxplayerB.cpp"
//#########################################################################################################################################
//#########################################################################################################################################
//-- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- - -- -  -- - -- - -- - -- - -- - -- - -- - -- - 

	// main() CONTINUED....

	// ADDED TO HOPEFULLY OVERLAY SDL GRAPHICS...
  m_config_video.layer = 10005;


#pragma mark OTHER SETUP  ~  ~  ~  ~  ~  ~  ~  ~  

	// SHOW HELP/USAGE IF BAD COMMAND LINE ARGS...
  if (optind >= argc) {
    print_usage();
    return 0;
  }

	// GET FILENAME/STREAM URL FROM COMMAND LINE ARGS
  m_filename = argv[optind];

	// INLINE FUNCTION TO PRINT ERRORS ABOUT MISSING FILES...
  auto PrintFileNotFound = [](const std::string& path)
  {
    printf("(OMXPLAYER) File \"%s\" not found.\n", path.c_str());
  };

	// CHECK TO MAKE SURE FILE EXISTS, SUBTITLES EXITS, SUBTITLE FONT EXISTS (EXIT IF NOT...)
	// ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = 
  bool filename_is_URL = IsURL(m_filename);

  if(!filename_is_URL && !IsPipe(m_filename) && !Exists(m_filename))
  {
    PrintFileNotFound(m_filename);
    return EXIT_FAILURE;
  }

  if(m_asked_for_font && !Exists(m_font_path))
  {
    PrintFileNotFound(m_font_path);
    return EXIT_FAILURE;
  }

  if(m_asked_for_italic_font && !Exists(m_italic_font_path))
  {
    PrintFileNotFound(m_italic_font_path);
    return EXIT_FAILURE;
  }

  if(m_has_external_subtitles && !Exists(m_external_subtitles_path))
  {
    PrintFileNotFound(m_external_subtitles_path);
    return EXIT_FAILURE;
  }
	// ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = ~  - = 

	// SET UP SUBITTLES FILESNAMES...
  if(!m_has_external_subtitles && !filename_is_URL)
  {
    auto subtitles_path = m_filename.substr(0, m_filename.find_last_of(".")) +
                          ".srt";

    if(Exists(subtitles_path))
    {
      m_external_subtitles_path = subtitles_path;
      m_has_external_subtitles = true;
    }
  }
    
	// CHECK IF IT'S A SOUND FILE TYPE (VS. VIDEO)
  bool m_audio_extension = false;
  const CStdString m_musicExtensions = ".nsv|.m4a|.flac|.aac|.strm|.pls|.rm|.rma|.mpa|.wav|.wma|.ogg|.mp3|.mp2|.m3u|.mod|.amf|.669|.dmf|.dsm|.far|.gdm|"
                 ".imf|.it|.m15|.med|.okt|.s3m|.stm|.sfx|.ult|.uni|.xm|.sid|.ac3|.dts|.cue|.aif|.aiff|.wpl|.ape|.mac|.mpc|.mp+|.mpp|.shn|.zip|.rar|"
                 ".wv|.nsf|.spc|.gym|.adx|.dsp|.adp|.ymf|.ast|.afc|.hps|.xsp|.xwav|.waa|.wvs|.wam|.gcm|.idsp|.mpdsp|.mss|.spt|.rsd|.mid|.kar|.sap|"
                 ".cmc|.cmr|.dmc|.mpt|.mpd|.rmt|.tmc|.tm8|.tm2|.oga|.url|.pxml|.tta|.rss|.cm3|.cms|.dlt|.brstm|.mka";
  if (m_filename.find_last_of(".") != string::npos)
  {
    CStdString extension = m_filename.substr(m_filename.find_last_of("."));
    if (!extension.IsEmpty() && m_musicExtensions.Find(extension.ToLower()) != -1)
      m_audio_extension = true;
  }
  if(m_gen_log) {
    CLog::SetLogLevel(LOG_LEVEL_DEBUG);
    CLog::Init("./");
  } else {
    CLog::SetLogLevel(LOG_LEVEL_NONE);
  }

	// ** INTIALIZE OMX INTERFACE **
  g_RBP.Initialize(); // I THINK THIS IS A HOST FOR THE OMX DLL 
  g_OMX.Initialize(); // INTIALIZE OMX CORE INTERFACE TO GPU

	// CLEAR THE BACKGROUND WITH THE GIVEN COLOR...
  //blank_background(0xffffffff);
  blank_background(m_blank_background);

	// CHECK THAT ENOUGH GPU MEMORY IS CONFIGURED...
  int gpu_mem = get_mem_gpu();
  int min_gpu_mem = 64;
  if (gpu_mem > 0 && gpu_mem < min_gpu_mem)
    printf("(OMXPLAYER) Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n", gpu_mem, min_gpu_mem);

	// ** CREATE OMX CLOCK **
  m_av_clock = new OMXClock();
  
  // ** INITIALIZE OMX CONTROL, PASS OMX CONTROL THE OTHER EXISTING OBJECTS... **
  int control_err = m_omxcontrol.init(
    m_av_clock,
    &m_player_audio,
    &m_player_subtitles,
    &m_omx_reader,
    m_dbus_name
  );
  
  // INTIIALIZE KEYBOARD IF ENABLED...
  if (false == m_no_keys)
  {
    m_keyboard = new Keyboard();
  }
  if (NULL != m_keyboard)
  {
    m_keyboard->setKeymap(keymap);
    m_keyboard->setDbusName(m_dbus_name);
  }

	//--------------------------------------------
	// ** OPEN MEDIA FILE (OR STREAM) **
  if(!m_omx_reader.Open(m_filename.c_str(), m_dump_format, m_config_audio.is_live, m_timeout, m_cookie.c_str(), m_user_agent.c_str(), m_lavfdopts.c_str(), m_avdict.c_str()))
    goto do_exit;
	//--------------------------------------------

	// IF THE FLAG IS SET TO ONLY SHOW THE VIDEO FORMAT, THEN SKIP TO THE EXIT ROUTINE NOW ----->
  if (m_dump_format_exit)
    goto do_exit;

	// DETERMINE WHICH TYPE OF MEDIA STREAMS ARE AVAILABLE...
  m_has_video     = m_omx_reader.VideoStreamCount();
  m_has_audio     = m_audio_index_use < 0 ? false : m_omx_reader.AudioStreamCount();
  m_has_subtitle  = m_has_external_subtitles ||
                    m_omx_reader.SubtitleStreamCount();
  m_loop          = m_loop && m_omx_reader.CanSeek();

	// WARN THAT NO VIDEO WILL BE SHOWN IF FILE IS AUDIO TYPE...
  if (m_audio_extension)
  {
    CLog::Log(LOGWARNING, "%s - Ignoring video in audio filetype:%s", __FUNCTION__, m_filename.c_str());
    m_has_video = false;
  }

	// THIS APPEARS TO BE SETTING FLAGS FOR 3D FILE TYPES...
  if(m_filename.find("3DSBS") != string::npos || m_filename.find("HSBS") != string::npos)
    m_3d = CONF_FLAGS_FORMAT_SBS;
  else if(m_filename.find("3DTAB") != string::npos || m_filename.find("HTAB") != string::npos)
    m_3d = CONF_FLAGS_FORMAT_TB;

  // 3d modes don't work without switch hdmi mode
  if (m_3d != CONF_FLAGS_FORMAT_NONE || m_NativeDeinterlace)
    m_refresh = true;

	// TURNS ON HDMI SYNC IN SOME CASES...?
  // you really don't want want to match refresh rate without hdmi clock sync
  if ((m_refresh || m_NativeDeinterlace) && !m_no_hdmi_clock_sync)
    m_config_video.hdmi_clock_sync = true;

	// ** INITIALIZE A/V CLOCK... **
  if(!m_av_clock->OMXInitialize())
    goto do_exit;

  //printf( "(OMXPLAYER) BEFORE HDMI SYNC TEST\n" );
  
	// EXIT IF HDMI SYNC FAILS...?  ----------------------->
  if(m_config_video.hdmi_clock_sync && !m_av_clock->HDMIClockSync())
    goto do_exit;
  
	// WHY IS IS CYCLING THROUGH A/V CLOCK STATES HERE...?
  m_av_clock->OMXStateIdle();
  m_av_clock->OMXStop();
  m_av_clock->OMXPause();

	// NOT SURE WHAT "HINTS" ARE...MAYBE DECODER CONFIG INFO...???
  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);
  m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_config_video.hints);

	// SET UP FPS...?
  if (m_fps > 0.0f)
    m_config_video.hints.fpsrate = m_fps * DVD_TIME_BASE, m_config_video.hints.fpsscale = DVD_TIME_BASE;

	// CHOOSE AUDIO STREAM
  if(m_audio_index_use > 0)
    m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_audio_index_use-1);
          
  // SET UP VIDEO MODE (IF VIDEO)...
  if(m_has_video && m_refresh)
  {
    memset(&tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
    m_BcmHost.vc_tv_get_display_state(&tv_state);

    SetVideoMode(m_config_video.hints.width, m_config_video.hints.height, m_config_video.hints.fpsrate, m_config_video.hints.fpsscale, m_3d);
  }
  // get display aspect
  TV_DISPLAY_STATE_T current_tv_state;
  memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
  m_BcmHost.vc_tv_get_display_state(&current_tv_state);
  if(current_tv_state.state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
    //HDMI or DVI on
    m_config_video.display_aspect = get_display_aspect_ratio((HDMI_ASPECT_T)current_tv_state.display.hdmi.aspect_ratio);
  } else {
    //composite on
    m_config_video.display_aspect = get_display_aspect_ratio((SDTV_ASPECT_T)current_tv_state.display.sdtv.display_options.aspect);
  }
  m_config_video.display_aspect *= (float)current_tv_state.display.hdmi.height/(float)current_tv_state.display.hdmi.width;

	// SET THE DISPLAY ORIENTATION...???
  if (m_orientation >= 0)
    m_config_video.hints.orientation = m_orientation;
    
  //printf( "(OMXPLAYER) BEFORE OPEN VIDEO STREAM\n" );

  // ** OPEN THE VIDEO STREAM IN THE VIDEO OBJECT **
  if(m_has_video && !m_player_video.Open(m_av_clock, m_config_video))
    goto do_exit;

  //printf( "(OMXPLAYER) BEFORE MAIN LOOP\n" );

	// SET UP SUBTITLES...
  if(m_has_subtitle || m_osd)
  {
    std::vector<Subtitle> external_subtitles;
    if(m_has_external_subtitles &&
       !ReadSrt(m_external_subtitles_path, external_subtitles))
    {
       puts("Unable to read the subtitle file.");
       goto do_exit;
    }

    if(!m_player_subtitles.Open(m_omx_reader.SubtitleStreamCount(),
                                std::move(external_subtitles),
                                m_font_path,
                                m_italic_font_path,
                                m_font_size,
                                m_centered,
                                m_ghost_box,
                                m_subtitle_lines,
                                m_config_video.display, m_config_video.layer + 1,
                                m_av_clock))
      goto do_exit;
    if(m_config_video.dst_rect.x2 > 0 && m_config_video.dst_rect.y2 > 0)
        m_player_subtitles.SetSubtitleRect(m_config_video.dst_rect.x1, m_config_video.dst_rect.y1, m_config_video.dst_rect.x2, m_config_video.dst_rect.y2);
  }

	// SET UP SUBTITLES...
  if(m_has_subtitle)
  {
    if(!m_has_external_subtitles)
    {
      if(m_subtitle_index != -1)
      {
        m_player_subtitles.SetActiveStream(
          std::min(m_subtitle_index, m_omx_reader.SubtitleStreamCount()-1));
      }
      m_player_subtitles.SetUseExternalSubtitles(false);
    }

    if(m_subtitle_index == -1 && !m_has_external_subtitles)
      m_player_subtitles.SetVisible(false);
  }

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);

	// SET UP AUDIO DEVICES...
  if (m_config_audio.device == "")
  {
    if (m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
      m_config_audio.device = "omx:hdmi";
    else
      m_config_audio.device = "omx:local";
  }

	// "ALSA" PROBABLY ENABLES USING USB AUDIO ADAPTERS...
  if(m_config_audio.device == "omx:alsa" && m_config_audio.subdevice.empty())
    m_config_audio.subdevice = "default";

	// CHECK IF HDMI CONNECTED DEVICE SUPPORTS AC3 OR DTS DIGITAL AUDIO PASSTHROUGH...
  if ((m_config_audio.hints.codec == AV_CODEC_ID_AC3 || m_config_audio.hints.codec == AV_CODEC_ID_EAC3) &&
      m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;
  if (m_config_audio.hints.codec == AV_CODEC_ID_DTS &&
      m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;

	// OPEN AUDIO STREAM WITH AUDIO DECODER/PLAYER CLASS...
  if(m_has_audio && !m_player_audio.Open(m_av_clock, m_config_audio, &m_omx_reader))
    goto do_exit;

	// SET STARTING VOLUME...
  if(m_has_audio)
  {
    m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
    if (m_Amplification)
      m_player_audio.SetDynamicRangeCompression(m_Amplification);
  }

	// NOT SURE WHAT THIS THRESHOLD IS FOR...???
  if (m_threshold < 0.0f)
    m_threshold = m_config_audio.is_live ? 0.7f : 0.2f;

  PrintSubtitleInfo();

	// RESET OMX CLOCK AND START PLAYING...???
  m_av_clock->OMXReset(m_has_video, m_has_audio);
  m_av_clock->OMXStateExecute();
  sentStarted = true;

  // *** DEBUG - ADDED THIS TO PAUSE ON START ***
  //m_av_clock->OMXStop();

  //printf( "(OMXPLAYER) LAUNCHED: DBUS=%s <***\n", m_dbus_name.c_str() );
  //printf( "(OMXPLAYER) \\FILE=%s <***\n", m_filename.c_str() );

  // DEFAULT PRELOADING VIDEO WINDOW OFF-SCREEN
  m_config_video.dst_rect.x1 = OFFSCREEN_X1; 
  m_config_video.dst_rect.y1 = OFFSCREEN_Y1;
  m_config_video.dst_rect.x2 = OFFSCREEN_X2;
  m_config_video.dst_rect.y2 = OFFSCREEN_Y2; //*/

//------------------------------------------------------------------------------------------------
#pragma mark MAIN LOOP  ~  ~  ~  ~  ~  ~  ~  ~  

  // MAIN LOOP... 
  while(!m_stop)
  {

    bool update = false; 

    // /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
    // *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + 
    //  + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + **
    // *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + 
    // *** DEBUG - SHOW MEDIA TIME *** 
    mediaTimeUSec = m_av_clock->OMXMediaTime() + (localLoopCount * videoLength * 1000);
    cycleCount++;
    
    long long videoStartTime = mediaTimeOffsetMSec + playlistStartTimeMSec;
    
    // DEBUG DISPLAY
    /*if( cycleCount%(66*3) == 0 )
      printf( "PAUS: %d  MEDIA_TIME: %f  CLOCK_TIME: %f  LOOP#: %ld  LOCAL#: %ld\n", 
              m_Pause, mediaTimeUSec/1000000.0, (getCurrentTimeMSec() - videoStartTime)/1000.0, loopCount, localLoopCount ); //*/
    
    WindowFrame testFrame = {0,0,1024,1280};
    testFrame.x = 1;
    
    if( startPause == true )
    {
      startPause = false;
      printf( "(OMXPLAYER) ** START PAUSE + *** + *** + *** + *** + *** + *** + *** + *** + **\n" );
      timelineState = STATE_USER_PAUSING;
    }
    
    if( startUnpause == true )
    {
      startUnpause = false;
      printf( "(OMXPLAYER) ** START UNPAUSE + *** + *** + *** + *** + *** + *** + *** + *** + **\n" );
      if( timelineState == STATE_USER_PAUSED )
      {
        timelineState = STATE_PRELOADING_PAUSED;
        //timelineState = STATE_USER_UNPAUSING;
      }
    }
    
    // SWITCH TO HANDLE PRELOADING OF VIDEO BASED ON SCHEDULING...
    switch( timelineState )
    { 
      //____________________________________________________________________________
      case STATE_STARTUP: //-------------------------------------------
      // DECIDE WHETHER TO GO DIRECTLY INTO PRELOADING STATE OR SEEK TO LOCATION...
      {
        // IF NO START TIME SET (STARTED MANUALLY FROM COMMAND LINE)...
        if( videoStartTime == 0 )
        {
          printf( "(OMXPLAYER) NO START TIME SET -- PLAY!\n" );
          timelineState = STATE_PLAYING;
        }
        else
        {
          // IF WE'RE ALREADY IN THE MIDDLE OF THE VIDEO...
          if( videoStartTime < getCurrentTimeMSec() ) // <-----------
          {
            // START IN THE MIDDLE OF THE VIDEO SOMEWHERE...
            timelineState = STATE_PRELOAD_SEEK;
          }
          else //*/
          {
            // START AT THE BEGINNING OF THE VIDEO...
            timelineState = STATE_PRELOADING;
          }        
        }

        /*printf( "(OMXPLAYER) :::~~~:::~~~:::~~~ PRELOADING........................................\n" );
        printf( "(OMXPLAYER) MEDIA TIME: %f\n", mediaTimeUSec ); //*/
        break;
      }

#define DEBUG_JUMP_FORWARD_SEC 20
#define SECONDS_TO_JUMP_AHEAD 2

      //____________________________________________________________________________
      case STATE_PRELOAD_SEEK: // ---------------------------------------
      // JUMP TO A FEW SECONDS LATER THAN WHERE PLAYBACK SHOULD CURRENTLY BE...
      {
        //  IF WE'RE SEEKING BEYOND THE FIRST ITERATION OF THE LOOP (NOTE: MAKE SURE THIS IS ONLY USED IN SEAMLESS LOOP MODE!)...
        if( (getCurrentTimeMSec() - videoStartTime) > videoLength )
          localLoopCount = (getCurrentTimeMSec() - videoStartTime) / videoLength; // <-- SET THE LOOP NUMBER FOR THE SEAMLESS LOOP TIMING SYSTEM

        // IN SEAMLESS LOOP MODE, CHECK TO SEE IF THE TARGET TIME IS NEAR OR BEYOND THE END OF THE LOOP...
        const int MARGIN_SEC = 2;
        if( m_seamless_loop == true  &&  ( (getCurrentTimeMSec() - videoStartTime) % videoLength ) / 1000 + SECONDS_TO_JUMP_AHEAD + MARGIN_SEC > (videoLength/1000) )
        {
          // START AT THE BEGINNING OF THE NEXT LOOP...
          localLoopCount++;
          timelineState = STATE_PRELOADING;
        }
        else
        {
          // IF SEEK ENABLED (THIS SHOULD ALWAYS BE ENABLED SINCE I'M NEVER PLAYING STREAMS)...
          if(m_omx_reader.CanSeek())  
          {
            // SEEK TO A COUPLE SECONDS AFTER WHAT SHOULD BE CURRENTLY PLAYING...
            m_incr = ( (getCurrentTimeMSec() - videoStartTime) % videoLength ) / 1000 + SECONDS_TO_JUMP_AHEAD; // m_incr VARIABLE UNIT IS SECONDS
            m_Pause = true;
            timelineState = STATE_PRELOADING_PAUSED;
          }
        }

        printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& STATE_PRELOAD_SEEK:   seek n_incr = %lld\n", m_incr ); //*/
        break;
      }

      //____________________________________________________________________________
      /*case STATE_PRELOAD_SEEK_PAUSE: //---------------------------------------
      // THIS IS CURRENTLY UNUSED!
      // PAUSE ON STARTUP -- IS THIS REALLY NEEDED?
      {
        //if( cycleCount%40 == 0 )
        //printf( "(OMXPLAYER) PRELOADING MEDIA TIME: %f\n", mediaTimeUSec ); 
        if( mediaTimeUSec > (DEBUG_JUMP_FORWARD_SEC * 1000 * 1000) + 2000000  ) // WAIT X SECONDS FOR VIDEO TO START RENDERING...
        {
          //printf( "(OMXPLAYER) PRELOADING MEDIA TIME: >>>> %f\n", mediaTimeUSec ); 
          m_Pause = true;
          //timelineState = STATE_PLAYING;
          timelineState = STATE_PRELOADING_PAUSED;
        }
        break;
      } //*/
        
      //____________________________________________________________________________
      case STATE_PRELOADING: //---------------------------------------
      // PAUSE VIDEO AS SOON AS IT PLAYS THE FIRST FRAME.
      {
        // DEBUG DISPLAY
        //if( cycleCount%40 == 0 )
          //printf( "(OMXPLAYER) PRELOADING MEDIA TIME: %f\n", mediaTimeUSec ); //*/
          //printf( "(OMXPLAYER) \\-- CURRENT TIME: %lld \n", getCurrentTimeMSec() );
    
    
        // WAIT FOR VIDEO TO REACH FIRST FRAME...
        //if( mediaTimeUSec > 2000000  ) // DEBUG!
        //if( mediaTimeUSec > 2000 * 1000  ) // DEBUG!
        if( mediaTimeUSec > 0  ) 
        {
          printf( "(OMXPLAYER) &&&&&&&& FINAL PRELOADING MEDIA TIME: %f\n", mediaTimeUSec ); 
          //printf( ":::~~~:::~~~:::~~~ PAUSED........................................\n" );
          //printf( "MEDIA TIME: %f\n", mediaTimeUSec ); 
          update = true;
          m_Pause = true;
          timelineState = STATE_PRELOADING_PAUSED_FROM_ZERO;
        } //*/
        break;
      }
      
      //____________________________________________________________________________
      case STATE_USER_PAUSING: //---------------------------------------
      // PAUSE VIDEO WHEN IT REACHES PAUSE TIME
      {
        const float DIV = 1.0;
        //if( (mediaTimeUSec/1000) + videoStartTime > pauseTimeMSec )
        if( getCurrentTimeMSec() - playlistStartTimeMSec > pauseTimeMSec )
        {
          //printf( "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& PRELOADING MEDIA TIME: %f\n", mediaTimeUSec ); //*/
          //printf( "(OMXPLAYER) MEDIA TIME: %f\n", mediaTimeUSec ); //*/
          //printf( "(OMXPLAYER)  ~~:::~~~ PAUSE LOCKED....... %.1f TARGET: %.1f\n", pauseTimeMSec/DIV, ((mediaTimeUSec/1000.0) + videoStartTime)/DIV ); //*/
          printf( "(OMXPLAYER)  ~~:::~~~ PAUSE LOCKED....... %.1f TARGET: %.1f 'OFFSET': %.1f \n", pauseTimeMSec/DIV, (getCurrentTimeMSec() - playlistStartTimeMSec)/DIV, playlistStartTimeMSec/DIV ); //*/
          update = true;
          m_Pause = true;
          timelineState = STATE_USER_PAUSED;
        }
        else
        {
          if( cycleCount%40 == 0 )
            printf( "(OMXPLAYER) WAITING FOR PAUSE: %.1f TARGET: %.1f  'OFFSET': %.1f\n", pauseTimeMSec/DIV, (getCurrentTimeMSec() - playlistStartTimeMSec)/DIV, playlistStartTimeMSec/DIV ); //*/
            //printf( "(OMXPLAYER) WAITING FOR PAUSE: %.1f TARGET: %.1f\n", pauseTimeMSec/DIV, ((mediaTimeUSec/1000.0) + videoStartTime)/DIV ); //*/
        }
        break;
      }
      
      //____________________________________________________________________________
      case STATE_USER_PAUSED: //---------------------------------------
      // VIDEO IS PAUSED
      {
        break;
      }
      
      //____________________________________________________________________________
      case STATE_USER_UNPAUSING: //---------------------------------------
      // THIS STATE PROBABLY NOT NEEDED 
      // WAIT FOR UNPAUSE TIME AND UNPAUSE VIDEO`
      {
        break;
      }
      
#define UNPAUSE_DELAY_MSEC 2
      
      //____________________________________________________________________________
      case STATE_PRELOADING_PAUSED_FROM_ZERO: //---------------------------------
      // WAIT FOR VIDEO MEDIA TIME TO MATCH ACTUAL TIME OFFSET AND THEN UNPAUSE VIDEO.
      // THIS IS EXACTLY THE SAME AS "STATE_PRELOADING_PAUSED", CAN THESE BE MERGED?
      //printf( "(OMXPLAYER) WAITING TO START:   %lld    %lld \n", (long long)(mediaTimeUSec/1000) + (long long)videoStartTime, getCurrentTimeMSec() );
      {
        //const int UNPAUSE_DELAY_MSEC = 2; // TAKES ABOUT 2 MSEC TO START PLAYING AGAIN
        if( (mediaTimeUSec/1000) + videoStartTime <= (getCurrentTimeMSec() + UNPAUSE_DELAY_MSEC) )
        {
          printf( "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& OMXPLAYER STATE_PRELOADING_PAUSED: ZERO  unpausing\n" );
          printf( ":::~~~:::~~~:::~~~ PLAYING........................................\n" );
          printf( "MEDIA TIME: %f\n", mediaTimeUSec );
          printf( "currentTimeMSec()=%lld   videoStartTime=%lld  videoLength=%lld\n", getCurrentTimeMSec(), videoStartTime, videoLength ); //*/
          update = true;
          m_Pause = false;
          timelineState = STATE_PLAYING;
          videoPositionChanged = true;
        }
        break;
      }

      //____________________________________________________________________________
      case STATE_PRELOADING_PAUSED: //---------------------------------
      // WAIT FOR VIDEO MEDIA TIME TO MATCH ACTUAL TIME OFFSET AND THEN UNPAUSE VIDEO.
      // THIS IS EXACTLY THE SAME AS "STATE_PRELOADING_PAUSED_FROM_ZERO", CAN THESE BE MERGED?
      {
        //const int UNPAUSE_DELAY_MSEC = 2; // TAKES ABOUT 2 MSEC TO START PLAYING AGAIN
        if( (mediaTimeUSec/1000) + videoStartTime < (getCurrentTimeMSec() + UNPAUSE_DELAY_MSEC) ) 
        {
          //printf( "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& OMXPLAYER STATE_PRELOADING_PAUSED:   unpausing\n" );
          update = true;
          m_Pause = false;
          timelineState = STATE_PLAYING;
          videoPositionChanged = true;
        }
        break;
      } //*/

      //____________________________________________________________________________
      case STATE_PLAYING: //-------------------------------------------
      // PLAYS VIDEO AND THROTTLES SPEED AS NECESSARY TO MATCH SCHEDULED TIME OFFSET.
      // STOPS VIDEO WHEN IT'S DONE PLAYING (UNLESS IN SEAMLESS LOOP MODE).
      {
        //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- STATE: PLAYING\n" );

        // IF SCHEDULING ENABLED...
        if( videoStartTime != 0 )
        {
          //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- VIDEOSTART TIME != 0 \n" );

          // THROTTLE SPEED TO KEEP IN SYNC WITH CLOCK (WHEN SYNC IS ON)...
          long long playbackTimeOffsetMSec = (long long)(mediaTimeUSec/1000) - (getCurrentTimeMSec()-videoStartTime);
          //if( cycleCount%5000 == 0 )
          //if( cycleCount%1200 == 0 )
          //if( cycleCount%800 == 0 )
          if( cycleCount%80 == 0 )
          {
            
          
          
            //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- CHECKING SYNC ALIGNMENT\n" );
            if( syncEnabled == false )
            {
              //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- THROTTLING DISABLED\n" );
            }
            else
            {
              const unsigned AVERAGE_ARRAY_MAX_SIZE = 50;
              timeOffsetArray.push_back( playbackTimeOffsetMSec );
              if( timeOffsetArray.size() > AVERAGE_ARRAY_MAX_SIZE )
                timeOffsetArray.pop_front();
              int sum = 0;  
              for( unsigned i=0; i<timeOffsetArray.size(); i++ )
                sum += timeOffsetArray[i];
              float currentAverageOffset = sum / (float) timeOffsetArray.size();
              
              //------------------------------
              //printf( "--(OMXPLAYER) TIME OFFSET -- AVERAGE: %f  THIS OFFSET: %lld \n", currentAverageOffset, playbackTimeOffsetMSec );

              //const long long MIN_OFFSET = -2; // <--------------
              //const long long MAX_OFFSET = 2; // <---------------
              const float MIN_OFFSET = -0.5; // <--------------
              const float MAX_OFFSET = 1.0; // <---------------

              const int MINIMUM_SAMPLES_BEFORE_ALLOWING_ADJUSTMENT = 2;
              if( timeOffsetArray.size() > MINIMUM_SAMPLES_BEFORE_ALLOWING_ADJUSTMENT )
              {
                if( currentAverageOffset > MAX_OFFSET )
                {
                  int slowdownSpeed = ceil( (playbackTimeOffsetMSec - MAX_OFFSET) / 4.0 );
                  SetSpeed( DVD_PLAYSPEED_NORMAL - slowdownSpeed ); // slowdownSpeed SHOULD BE BETWEEN 1 and 7
                  //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- SPEED SLOW %f %d\n", playbackTimeOffsetMSec - MAX_OFFSET, slowdownSpeed ); // DEBUG!
                }
                else
                {
                  if( currentAverageOffset < MIN_OFFSET )
                  {
                    int speedupSpeed = ceil( (MIN_OFFSET - playbackTimeOffsetMSec) / 4.0 );
                    SetSpeed( DVD_PLAYSPEED_NORMAL + speedupSpeed ); // speedupSpeed SHOULD BE BETWEEN 1 and 7
                    //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- SPEED FAST %f %d\n", MIN_OFFSET - playbackTimeOffsetMSec, speedupSpeed ); // DEBUG!
                  }
                  else
                  {
                    SetSpeed( DVD_PLAYSPEED_NORMAL );
                    //printf( "(OMXPLAYER) &&&&&&&&&&&&&&&&&&--- SPEED NORMAL\n" ); // DEBUG!
                  }
                }
              }
            }
          } //*/

          // QUIT OMXPLAYER IF PAST END OF SCHEDULED VIDEO...          
          const long long VIDEO_EARLY_ENDING_MSEC = 20;
          //const long long VIDEO_EARLY_ENDING_MSEC = 100;
          long long videoEndTime = videoStartTime + videoLength - VIDEO_EARLY_ENDING_MSEC;
          //long long videoEndTime = (mediaTime/1000) + videoStartTime + videoLength;
          //printf( "CURRENT TIME: %lld  videoEndTime: %lld\n", getCurrentTimeMSec(), videoEndTime );
          //const int EARLY_VIDEO_CUTOFF_MSEC = 100; // DEBUG VALUE TO TEST HOW MUCH GAP IS NEEDED, PROBABLY SHOULD BE ZERO
          const int EARLY_VIDEO_CUTOFF_MSEC = 0; // DEBUG VALUE TO TEST HOW MUCH GAP IS NEEDED, PROBABLY SHOULD BE ZERO
          if( m_seamless_loop == false  &&  (videoEndTime - EARLY_VIDEO_CUTOFF_MSEC) < getCurrentTimeMSec() )
          {
            //printf( ":::~~~:::~~~:::~~~ STOPPED........................................\n" );
            //printf( "MEDIA TIME: %f\n", mediaTimeUSec );
            timelineState = STATE_FINISHED_PLAYING;
            videoPositionChanged = true;

            // SKIP LAST STATE?
            update = true;
            m_stop = true;
            goto do_exit;
          } //*/
          
        } // IF videoStartTime NOT ZERO

        break;
      }
        
      //____________________________________________________________________________
      case STATE_FINISHED_PLAYING: //----------------------------------
      // THIS IS PROBABLY NOT NEEDED, BUT KEEPING IT NOW JUST IN CASE AN END STATE IS NEEDED
      {
        update = true;
        m_stop = true;
        goto do_exit; // ------------------> JUMP TO END OF LOOP
        break;
      }
      
    } // SWITCH -----------------
    
    
    // HIDE VIDEO OFF-SCREEN EXCEPT WHEN IT'S SCHEDULED TO PLAY...
    if( m_has_video == true  &&  videoPositionChanged == true )
    //if( 0 ) // DEBUG!!!!!!!!!!!!!!!!
    {
      CRect newVideoPosition;    
      videoPositionChanged = false;
      if( timelineState == STATE_PLAYING ) // <---------- NORMAL
      {
        // MOVE VIDEO TO CORRECT POSITION WHEN PLAYING...
        newVideoPosition.x1 = storedVideo.x1;
        newVideoPosition.y1 = storedVideo.y1;
        newVideoPosition.x2 = storedVideo.x2;
        newVideoPosition.y2 = storedVideo.y2;

        m_config_video.dst_rect = newVideoPosition;

        AspectMode currentAspectMode = ASPECT_LETTERBOX;
        switch( m_config_video.aspectMode )
        {
          case 0: currentAspectMode = ASPECT_LETTERBOX; break; // UNCLEAR WHAT THE VALUE ZERO MEANS, BUT WE'RE TREATING IT AS LETTERBOX
          case 1: currentAspectMode = ASPECT_LETTERBOX; break; // THIS IS THE REAL LETTERBOX
          case 2: currentAspectMode = ASPECT_NATURAL; break;
          case 3: currentAspectMode = ASPECT_STRETCH; break;
        } //*/
        float scaleFactor = 1.00;
        // IF MENU IS OPEN (THIS SHOULD ALWAYS WORK FOR NOW, BUT ISN'T IDEAL)...
        if( storedVideo.y2 - storedVideo.y1 < 450 )
           scaleFactor = 0.3;
      
        // SETUP SOURCE FRAME AND SWAP HEIGHT/WIDTH WHEN ROTATED...        
        WindowFrame sourceFrame = { 0, 0, m_config_video.hints.width, m_config_video.hints.height };
        //printf( "(OMXPLAYER) ORIENTATION: %d \n", m_orientation );
        if( m_orientation == 90  ||  m_orientation == 270 )        
          sourceFrame = { 0, 0, m_config_video.hints.height, m_config_video.hints.width };

        // CALCULATED SOURCE AND DEST FRAMES BASED ON ASPECT MODE
        WindowFrame adjustedSourceRect = getSourceCopyRect( sourceFrame, WindowFrame(newVideoPosition), currentAspectMode, scaleFactor );
        WindowFrame adjustedDestRect = getDestCopyRect( sourceFrame, WindowFrame(newVideoPosition), currentAspectMode, scaleFactor );

        // ADJUST SOURCE AND DEST FRAMES BASED ON VIDEO WALL SETTINGS
        WindowFrame finalSourceRect = getVideoWallSourceRect( adjustedSourceRect, adjustedDestRect, WindowFrame( storedVideo ), videoWallConfig );
        WindowFrame finalDestRect = getVideoWallDestRect( adjustedSourceRect, adjustedDestRect, WindowFrame( storedVideo ), videoWallConfig );

        m_config_video.src_rect = finalSourceRect.getCRect();
        m_config_video.dst_rect = finalDestRect.getCRect(); 

        if( m_orientation == 90  ||  m_orientation == 270 )        
        {
          int temp1 = m_config_video.src_rect.x1;
          int temp2 = m_config_video.src_rect.x2;
          m_config_video.src_rect.x1 = m_config_video.src_rect.y1;
          m_config_video.src_rect.x2 = m_config_video.src_rect.y2;
          m_config_video.src_rect.y1 = temp1;
          m_config_video.src_rect.y2 = temp2;
        } 

        m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
        m_player_subtitles.SetSubtitleRect(m_config_video.dst_rect.x1, m_config_video.dst_rect.y1, m_config_video.dst_rect.x2, m_config_video.dst_rect.y2); //*/
      }
      else 
      {
        // MOVE VIDEO OFF SCREEN WHEN PRELOADING...
        newVideoPosition.x1 = OFFSCREEN_X1; // SET PRELOADING VIDEO WINDOW OFF-SCREEN
        newVideoPosition.y1 = OFFSCREEN_Y1;
        newVideoPosition.x2 = OFFSCREEN_X2;
        newVideoPosition.y2 = OFFSCREEN_Y2; 

        m_config_video.dst_rect = newVideoPosition;

        m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
        m_player_subtitles.SetSubtitleRect(m_config_video.dst_rect.x1, m_config_video.dst_rect.y1, m_config_video.dst_rect.x2, m_config_video.dst_rect.y2); //*/ 
      }

    }
    //*/
   
    // *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + 
    //  + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + **
    // *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + *** + 
    // /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/


  
    if(g_abort)
      goto do_exit;

		// LIMITS HOW FAST "UPDATES" HAPPEN (I ASSUME AN UPDATE IS A FRAME?)...
		// MOST OF THE CODE AFTER THIS IS SKIPPED UNLESS "update" IS TRUE.
		// ..THIS SEEMS TO BE UPDATING AT 50FPS (20MSEC)
    double now = m_av_clock->GetAbsoluteClock();
    if (m_last_check_time == 0.0 || m_last_check_time + DVD_MSEC_TO_TIME(20) <= now) 
    {
      update = true;
      m_last_check_time = now;
    }


		// PROCESS INCOMING KEYBOARD OR DBUS COMMANDS...
    if (update) {
       OMXControlResult result = control_err
                               ? (OMXControlResult)(m_keyboard ? m_keyboard->getEvent() : KeyConfig::ACTION_BLANK)
                               : m_omxcontrol.getEvent();
       double oldPos, newPos;

      // HUGE SWITCH STATEMENT FOR ALL THE "KEY" COMMANDS (WHICH ACTUALLY MAY BE GENERATED BY DBUS *OR* KEYBOARD)...
      switch(result.getKey())
      {
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_SHOW_INFO:
          m_tv_show_info = !m_tv_show_info; 
          vc_tv_show_info(m_tv_show_info);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_PREVIOUS_AUDIO:
          if(m_has_audio)
          {
            int new_index = m_omx_reader.GetAudioIndex() - 1;
            if(new_index >= 0)
            {
              m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, new_index);
              DISPLAY_TEXT_SHORT(
                strprintf("Audio stream: %d", m_omx_reader.GetAudioIndex() + 1));
            }
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_NEXT_AUDIO:
          if(m_has_audio)
          {
            m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_omx_reader.GetAudioIndex() + 1);
            DISPLAY_TEXT_SHORT(
              strprintf("Audio stream: %d", m_omx_reader.GetAudioIndex() + 1));
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_PREVIOUS_CHAPTER:
          if(m_omx_reader.GetChapterCount() > 0)
          {
            m_omx_reader.SeekChapter(m_omx_reader.GetChapter() - 1, &startpts);
            DISPLAY_TEXT_LONG(strprintf("Chapter %d", m_omx_reader.GetChapter()));
            FlushStreams(startpts);
            m_seek_flush = true;
            m_chapter_seek = true;
          }
          else
          {
            m_incr = -600.0;
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_NEXT_CHAPTER:
          if(m_omx_reader.GetChapterCount() > 0)
          {
            m_omx_reader.SeekChapter(m_omx_reader.GetChapter() + 1, &startpts);
            DISPLAY_TEXT_LONG(strprintf("Chapter %d", m_omx_reader.GetChapter()));
            FlushStreams(startpts);
            m_seek_flush = true;
            m_chapter_seek = true;
          }
          else
          {
            m_incr = 600.0;
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_PREVIOUS_SUBTITLE:
          if(m_has_subtitle)
          {
            if(!m_player_subtitles.GetUseExternalSubtitles())
            {
              if (m_player_subtitles.GetActiveStream() == 0)
              {
                if(m_has_external_subtitles)
                {
                  DISPLAY_TEXT_SHORT("Subtitle file:\n" + m_external_subtitles_path);
                  m_player_subtitles.SetUseExternalSubtitles(true);
                }
              }
              else
              {
                auto new_index = m_player_subtitles.GetActiveStream()-1;
                DISPLAY_TEXT_SHORT(strprintf("Subtitle stream: %d", new_index+1));
                m_player_subtitles.SetActiveStream(new_index);
              }
            }

            m_player_subtitles.SetVisible(true);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_NEXT_SUBTITLE:
          if(m_has_subtitle)
          {
            if(m_player_subtitles.GetUseExternalSubtitles())
            {
              if(m_omx_reader.SubtitleStreamCount())
              {
                assert(m_player_subtitles.GetActiveStream() == 0);
                DISPLAY_TEXT_SHORT("Subtitle stream: 1");
                m_player_subtitles.SetUseExternalSubtitles(false);
              }
            }
            else
            {
              auto new_index = m_player_subtitles.GetActiveStream()+1;
              if(new_index < (size_t) m_omx_reader.SubtitleStreamCount())
              {
                DISPLAY_TEXT_SHORT(strprintf("Subtitle stream: %d", new_index+1));
                m_player_subtitles.SetActiveStream(new_index);
              }
            }

            m_player_subtitles.SetVisible(true);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_TOGGLE_SUBTITLE:
          if(m_has_subtitle)
          {
            m_player_subtitles.SetVisible(!m_player_subtitles.GetVisible());
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_HIDE_SUBTITLES:
          if(m_has_subtitle)
          {
            m_player_subtitles.SetVisible(false);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_SHOW_SUBTITLES:
          if(m_has_subtitle)
          {
            m_player_subtitles.SetVisible(true);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_DECREASE_SUBTITLE_DELAY:
          if(m_has_subtitle && m_player_subtitles.GetVisible())
          {
            auto new_delay = m_player_subtitles.GetDelay() - 250;
            DISPLAY_TEXT_SHORT(strprintf("Subtitle delay: %d ms", new_delay));
            m_player_subtitles.SetDelay(new_delay);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_INCREASE_SUBTITLE_DELAY:
          if(m_has_subtitle && m_player_subtitles.GetVisible())
          {
            auto new_delay = m_player_subtitles.GetDelay() + 250;
            DISPLAY_TEXT_SHORT(strprintf("Subtitle delay: %d ms", new_delay));
            m_player_subtitles.SetDelay(new_delay);
            PrintSubtitleInfo();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_EXIT:
          m_stop = true;
          goto do_exit;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_DECREASE_SPEED:
          if (playspeed_current < playspeed_slow_min || playspeed_current > playspeed_slow_max)
            playspeed_current = playspeed_slow_max-1;
          playspeed_current = std::max(playspeed_current-1, playspeed_slow_min);
          SetSpeed(playspeeds[playspeed_current]);
          DISPLAY_TEXT_SHORT(
            strprintf("Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f));
          printf("(OMXPLAYER) Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
          m_Pause = false;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_INCREASE_SPEED:
          if (playspeed_current < playspeed_slow_min || playspeed_current > playspeed_slow_max)
            playspeed_current = playspeed_slow_max-1;
          playspeed_current = std::min(playspeed_current+1, playspeed_slow_max);
          SetSpeed(playspeeds[playspeed_current]);
          DISPLAY_TEXT_SHORT(
            strprintf("Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f));
          printf("(OMXPLAYER) Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
          m_Pause = false;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_REWIND:
          if (playspeed_current >= playspeed_ff_min && playspeed_current <= playspeed_ff_max)
          {
            playspeed_current = playspeed_normal;
            m_seek_flush = true;
          }
          else if (playspeed_current < playspeed_rew_max || playspeed_current > playspeed_rew_min)
            playspeed_current = playspeed_rew_min;
          else
            playspeed_current = std::max(playspeed_current-1, playspeed_rew_max);
          SetSpeed(playspeeds[playspeed_current]);
          DISPLAY_TEXT_SHORT(
            strprintf("Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f));
          printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
          m_Pause = false;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_FAST_FORWARD:
          if (playspeed_current >= playspeed_rew_max && playspeed_current <= playspeed_rew_min)
          {
            playspeed_current = playspeed_normal;
            m_seek_flush = true;
          }
          else if (playspeed_current < playspeed_ff_min || playspeed_current > playspeed_ff_max)
            playspeed_current = playspeed_ff_min;
          else
            playspeed_current = std::min(playspeed_current+1, playspeed_ff_max);
          SetSpeed(playspeeds[playspeed_current]);
          DISPLAY_TEXT_SHORT(
            strprintf("Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f));
          printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
          m_Pause = false;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_STEP:
          m_av_clock->OMXStep(); // <--- TELL CLOCK TO STEP
          printf("Step\n");
          {
            auto t = (unsigned) (m_av_clock->OMXMediaTime()*1e-3);
            auto dur = m_omx_reader.GetStreamLength() / 1000;
            DISPLAY_TEXT_SHORT(
              strprintf("Step\n%02d:%02d:%02d.%03d / %02d:%02d:%02d",
                (t/3600000), (t/60000)%60, (t/1000)%60, t%1000,
                (dur/3600), (dur/60)%60, dur%60));
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_BACK_SMALL:
          if(m_omx_reader.CanSeek()) m_incr = -30.0;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_FORWARD_SMALL:
          if(m_omx_reader.CanSeek()) m_incr = 30.0;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_FORWARD_LARGE:
          if(m_omx_reader.CanSeek()) m_incr = 600.0;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_BACK_LARGE:
          if(m_omx_reader.CanSeek()) m_incr = -600.0;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_RELATIVE:
            m_incr = result.getArg() * 1e-6;
            break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SEEK_ABSOLUTE:
            newPos = result.getArg() * 1e-6;
            oldPos = m_av_clock->OMXMediaTime()*1e-6;
            m_incr = newPos - oldPos;
            break;

  #pragma mark SET PLAYLIST START TIME
        // ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ 
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SET_PLAYLIST_START_TIME:
	  			// MY NEW DBUS COMMAND TO SET THE PLAYLIST_START_TIME
          printf( "(OMXPLAYER) ACTION_SET_PLAYLIST_START_TIME T R I G G E R E D\n" );
          printf( "(OMXPLAYER) X X X X X X X X X X X X X X X X X X X X X X X X \n" );
          playlistStartTimeMSec = result.getArg();	
          syncEnabled = true;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_SET_SYNC_MODE:
	  			// MY NEW DBUS COMMAND TO SET THE SYNC MODE
          printf( "(OMXPLAYER) ACTION_SET_SYNC_MODE T R I G G E R E D\n" );
          if( result.getArg() == 0 )
            syncEnabled = false;
          else
            syncEnabled = true;
          //syncEnabled = result.getArg();
          break;
        // ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ ~~ ^ 
        
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_PLAY:
          m_Pause=false;
          if(m_has_subtitle)
          {
            m_player_subtitles.Resume();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_PAUSE:
          m_Pause=true;
          if(m_has_subtitle)
          {
            m_player_subtitles.Pause();
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      	// |- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -||- -|
        case KeyConfig::ACTION_PLAYPAUSE:
          m_Pause = !m_Pause;
          if (m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_NORMAL && m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_PAUSE)
          {
            printf("(OMXPLAYER) resume\n");
            playspeed_current = playspeed_normal;
            SetSpeed(playspeeds[playspeed_current]);
            m_seek_flush = true;
          }
          if(m_Pause)
          { 
            if(m_has_subtitle)
              m_player_subtitles.Pause(); 

            auto t = (unsigned) (m_av_clock->OMXMediaTime()*1e-6);
            auto dur = m_omx_reader.GetStreamLength() / 1000;
            DISPLAY_TEXT_LONG(strprintf("Pause\n%02d:%02d:%02d / %02d:%02d:%02d",
              (t/3600), (t/60)%60, t%60, (dur/3600), (dur/60)%60, dur%60));
          }
          else 
          {
            if(m_has_subtitle)
              m_player_subtitles.Resume();

            auto t = (unsigned) (m_av_clock->OMXMediaTime()*1e-6);
            auto dur = m_omx_reader.GetStreamLength() / 1000;
            DISPLAY_TEXT_SHORT(strprintf("Play\n%02d:%02d:%02d / %02d:%02d:%02d",
              (t/3600), (t/60)%60, t%60, (dur/3600), (dur/60)%60, dur%60));
          }
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_SET_ALPHA:
            m_player_video.SetAlpha(result.getArg());
            break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_MOVE_VIDEO:
          sscanf(result.getWinArg(), "%f %f %f %f", &storedVideo.x1, &storedVideo.y1, &storedVideo.x2, &storedVideo.y2);
          videoPositionChanged = true;
          //sscanf(result.getWinArg(), "%f %f %f %f", &m_config_video.dst_rect.x1, &m_config_video.dst_rect.y1, &m_config_video.dst_rect.x2, &m_config_video.dst_rect.y2);
          //m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
          //m_player_subtitles.SetSubtitleRect(m_config_video.dst_rect.x1, m_config_video.dst_rect.y1, m_config_video.dst_rect.x2, m_config_video.dst_rect.y2);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_CROP_VIDEO:
          sscanf(result.getWinArg(), "%f %f %f %f", &m_config_video.src_rect.x1, &m_config_video.src_rect.y1, &m_config_video.src_rect.x2, &m_config_video.src_rect.y2);
          m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
          videoPositionChanged = true;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_HIDE_VIDEO:
          // set alpha to minimum
          m_player_video.SetAlpha(0);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_UNHIDE_VIDEO:
          // set alpha to maximum
          m_player_video.SetAlpha(255);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_SET_ASPECT_MODE:
          if (result.getWinArg()) {
            if (!strcasecmp(result.getWinArg(), "letterbox"))
              m_config_video.aspectMode = 1;
            else if (!strcasecmp(result.getWinArg(), "fill"))
              m_config_video.aspectMode = 2;
            else if (!strcasecmp(result.getWinArg(), "stretch"))
              m_config_video.aspectMode = 3;
            else
              m_config_video.aspectMode = 0;
            m_player_video.SetVideoRect(m_config_video.aspectMode);
          }
          videoPositionChanged = true;
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_DECREASE_VOLUME:
          m_Volume -= 300;
          m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
          DISPLAY_TEXT_SHORT(strprintf("Volume: %.2f dB",
            m_Volume / 100.0f));
          printf("(OMXPLAYER) Current Volume: %.2fdB\n", m_Volume / 100.0f);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        case KeyConfig::ACTION_INCREASE_VOLUME:
          m_Volume += 300;
          m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
          DISPLAY_TEXT_SHORT(strprintf("Volume: %.2f dB",
            m_Volume / 100.0f));
          printf("(OMXPLAYER) Current Volume: %.2fdB\n", m_Volume / 100.0f);
          break;
      	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        default:
          break;
      } // SWITCH FOR KEY COMMANDS
    } // IF UPDATE
    //--------------------------------------------------------------------------------------------------------

		// SLEEP IF IDLE (I ASSUME IDLE MEANS NOT PLAYING??)...
    if (idle)
    {
      usleep(10000);
      continue;
    }

    //---------------------------------------------------------------------------------------------------
		// RESPOND TO COMMANDS FROM THE SYNC THREAD...    
    /*switch( syncCommand )
    {
    	case SYNC_PLAY:
				m_av_clock->OMXResume();
				m_Pause = false;
    		break;
    	case SYNC_PAUSE:
				m_av_clock->OMXPause();
				m_Pause = true;
				syncCommand = SYNC_NO_COMMAND;
    		break;
    	case SYNC_NO_COMMAND:
    		break;
    } //*/
    
		// HANDLE SEEK ('m_incr' INDICATES THE SEEK AMOUNT/DIRECTION IN FLOAT SECONDS)...
    if(m_seek_flush || m_incr != 0) 
    {
      double seek_pos     = 0;
      double pts          = 0;

      if(m_has_subtitle)
        m_player_subtitles.Pause();

      if (!m_chapter_seek)
      {
        pts = m_av_clock->OMXMediaTime();

        seek_pos = (pts ? pts / DVD_TIME_BASE : last_seek_pos) + m_incr;
        last_seek_pos = seek_pos;

        seek_pos *= 1000.0;

				// SEEK THE READER TO THE GIVEN POSITION...
        if(m_omx_reader.SeekTime((int)seek_pos, m_incr < 0.0f, &startpts))
        {
          unsigned t = (unsigned)(startpts*1e-6);
          auto dur = m_omx_reader.GetStreamLength() / 1000;
          DISPLAY_TEXT_LONG(strprintf("Seek\n%02d:%02d:%02d / %02d:%02d:%02d",
              (t/3600), (t/60)%60, t%60, (dur/3600), (dur/60)%60, dur%60));
          printf("(OMXPLAYER) Seek to: %02d:%02d:%02d\n", (t/3600), (t/60)%60, t%60);
          FlushStreams(startpts);
        }
      }

      sentStarted = false;

      if (m_omx_reader.IsEof())
        goto do_exit;  

      // Quick reset to reduce delay during loop & seek.
      if (m_has_video && !m_player_video.Reset())
        goto do_exit;

      CLog::Log(LOGDEBUG, "(OMXPLAYER) Seeked %.0f %.0f %.0f\n", DVD_MSEC_TO_TIME(seek_pos), startpts, m_av_clock->OMXMediaTime());

			// UNCLEAR WHY THERE'S ANOTHER PAUSE COMMAND HERE?? (THIS IS NOT A TOGGLE, IT'S AN ACTUAL PAUSE)...
      m_av_clock->OMXPause();

      if(m_has_subtitle)
        m_player_subtitles.Resume();
      m_packet_after_seek = false;
      m_seek_flush = false;
      m_incr = 0;
    } // END OF SEEK SECTION ----/
    else if(m_packet_after_seek && TRICKPLAY(m_av_clock->OMXPlaySpeed())) // TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/FAST FORWARD (NOT NORMAL PLAY)
    {
    	// - - - - - - - - - - - - - - - - - - - - - - 
    	// UNCLEAR WHAT THIS SECTION DOES...
    	// IT ALSO SEEKS BUT I'M UNCLEAR WHY IT'S NEEDED IN ADDITION TO THE SEEK ABOVE
    
      double seek_pos     = 0;
      double pts          = 0;

      pts = m_av_clock->OMXMediaTime();
      seek_pos = (pts / DVD_TIME_BASE);

      seek_pos *= 1000.0;

			// SEEK THE READER TO THE GIVEN POSITION...
      if(m_omx_reader.SeekTime((int)seek_pos, m_av_clock->OMXPlaySpeed() < 0, &startpts))
        ; //FlushStreams(DVD_NOPTS_VALUE);

      CLog::Log(LOGDEBUG, "Seeked %.0f %.0f %.0f\n", DVD_MSEC_TO_TIME(seek_pos), startpts, m_av_clock->OMXMediaTime());

      //unsigned t = (unsigned)(startpts*1e-6);
      unsigned t = (unsigned)(pts*1e-6);
      printf("(OMXPLAYER) Seek to: %02d:%02d:%02d\n", (t/3600), (t/60)%60, t%60);
      m_packet_after_seek = false;
    } 
    // END OF "SEEK" SECTION
    //--------------------------------------------------------------------------

		// ERROR HANDLING...
    /* player got in an error state */
    if(m_player_audio.Error())
    {
      printf("(OMXPLAYER) audio player error. emergency exit!!!\n");
      goto do_exit;
    }

#pragma mark AUDIO VIDEO BUFFERS
		// ---------------------------------------------------------------
		// CHECK/FILL/THROTTLE AUDIO AND VIDEO BUFFERS (?)
    if (update)
    {
      /* when the video/audio fifos are low, we pause clock, when high we resume */
      double stamp = m_av_clock->OMXMediaTime();
      double audio_pts = m_player_audio.GetCurrentPTS();
      double video_pts = m_player_video.GetCurrentPTS();

			// IF PAUSED...
      if (0 && m_av_clock->OMXIsPaused())
      {
      	// CHOOSE WHETHER TO USE AUDIO OR VIDEO TIME AS TIMESTAMP BASED ON..?
        double old_stamp = stamp;
        if (audio_pts != DVD_NOPTS_VALUE && (stamp == 0 || audio_pts < stamp))
          stamp = audio_pts;
        if (video_pts != DVD_NOPTS_VALUE && (stamp == 0 || video_pts < stamp))
          stamp = video_pts;
          
        // IF THE TIME STAMP HAS CHANGED...
        if (old_stamp != stamp)
        {
        	// SET THE MEDIA TIME, THEN GET THAT TIME AS THE TIMESTAMP?
          m_av_clock->OMXMediaTime(stamp);
          stamp = m_av_clock->OMXMediaTime();
        }
      }

			// ???
      float audio_fifo = audio_pts == DVD_NOPTS_VALUE ? 0.0f : audio_pts / DVD_TIME_BASE - stamp * 1e-6;
      float video_fifo = video_pts == DVD_NOPTS_VALUE ? 0.0f : video_pts / DVD_TIME_BASE - stamp * 1e-6;
      float threshold = std::min(0.1f, (float)m_player_audio.GetCacheTotal() * 0.1f);
      bool audio_fifo_low = false, video_fifo_low = false, audio_fifo_high = false, video_fifo_high = false;

      //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			// DEBUG DISPLAY OF DECODER STATS...
      /*if(m_stats)
      {
        static int count;
        if ((count++ & 7) == 0)
           printf("M:%8.0f V:%6.2fs %6dk/%6dk A:%6.2f %6.02fs/%6.02fs Cv:%6dk Ca:%6dk                            \r", stamp,
               video_fifo, (m_player_video.GetDecoderBufferSize()-m_player_video.GetDecoderFreeSpace())>>10, m_player_video.GetDecoderBufferSize()>>10,
               audio_fifo, m_player_audio.GetDelay(), m_player_audio.GetCacheTotal(),
               m_player_video.GetCached()>>10, m_player_audio.GetCached()>>10);
      } //*/
      //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 
			// DEBUG DISPLAY OF RENDERING FIFO INFORMATION...?
      if(m_tv_show_info)
      {
        static unsigned count;
        if ((count++ & 7) == 0)
        {
          char response[80];
          if (m_player_video.GetDecoderBufferSize() && m_player_audio.GetCacheTotal())
            vc_gencmd(response, sizeof response, "render_bar 4 video_fifo %d %d %d %d",
                (int)(100.0*m_player_video.GetDecoderBufferSize()-m_player_video.GetDecoderFreeSpace())/m_player_video.GetDecoderBufferSize(),
                (int)(100.0*video_fifo/m_player_audio.GetCacheTotal()),
                0, 100);
          if (m_player_audio.GetCacheTotal())
            vc_gencmd(response, sizeof response, "render_bar 5 audio_fifo %d %d %d %d",
                (int)(100.0*audio_fifo/m_player_audio.GetCacheTotal()),
                (int)(100.0*m_player_audio.GetDelay()/m_player_audio.GetCacheTotal()),
                0, 100);
          vc_gencmd(response, sizeof response, "render_bar 6 video_queue %d %d %d %d",
                m_player_video.GetLevel(), 0, 0, 100);
          vc_gencmd(response, sizeof response, "render_bar 7 audio_queue %d %d %d %d",
                m_player_audio.GetLevel(), 0, 0, 100);
        }
      }

			// ???
      if (audio_pts != DVD_NOPTS_VALUE)
      {
        audio_fifo_low = m_has_audio && audio_fifo < threshold;
        audio_fifo_high = !m_has_audio || (audio_pts != DVD_NOPTS_VALUE && audio_fifo > m_threshold);
      }
      if (video_pts != DVD_NOPTS_VALUE)
      {
        video_fifo_low = m_has_video && video_fifo < threshold;
        video_fifo_high = !m_has_video || (video_pts != DVD_NOPTS_VALUE && video_fifo > m_threshold);
      }
      CLog::Log(LOGDEBUG, "Normal M:%.0f (A:%.0f V:%.0f) P:%d A:%.2f V:%.2f/T:%.2f (%d,%d,%d,%d) A:%d%% V:%d%% (%.2f,%.2f)\n", stamp, audio_pts, video_pts, m_av_clock->OMXIsPaused(), 
        audio_pts == DVD_NOPTS_VALUE ? 0.0:audio_fifo, video_pts == DVD_NOPTS_VALUE ? 0.0:video_fifo, m_threshold, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high,
        m_player_audio.GetLevel(), m_player_video.GetLevel(), m_player_audio.GetDelay(), (float)m_player_audio.GetCacheTotal());

      // keep latency under control by adjusting clock (and so resampling audio)
      if (m_config_audio.is_live)
      {
        float latency = DVD_NOPTS_VALUE;
        if (m_has_audio && audio_pts != DVD_NOPTS_VALUE)
          latency = audio_fifo;
        else if (!m_has_audio && m_has_video && video_pts != DVD_NOPTS_VALUE)
          latency = video_fifo;
        if (!m_Pause && latency != DVD_NOPTS_VALUE)
        {
          if (m_av_clock->OMXIsPaused())
          {
            if (latency > m_threshold)
            {
              CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
              m_av_clock->OMXResume();
              m_latency = latency;
            }
          }
          else
          {
            m_latency = m_latency*0.99f + latency*0.01f;
            float speed = 1.0f;
            if (m_latency < 0.5f*m_threshold)
              speed = 0.990f;
            else if (m_latency < 0.9f*m_threshold)
              speed = 0.999f;
            else if (m_latency > 2.0f*m_threshold)
              speed = 1.010f;
            else if (m_latency > 1.1f*m_threshold)
              speed = 1.001f;

            m_av_clock->OMXSetSpeed(S(speed));
            m_av_clock->OMXSetSpeed(S(speed), true, true);
            CLog::Log(LOGDEBUG, "Live: %.2f (%.2f) S:%.3f T:%.2f\n", m_latency, latency, speed, m_threshold);
          }
        }
      }
			// TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/FAST FORWARD (NOT NORMAL PLAY)
      else if(!m_Pause && (m_omx_reader.IsEof() || m_omx_pkt || TRICKPLAY(m_av_clock->OMXPlaySpeed()) || (audio_fifo_high && video_fifo_high)))
      {
        if (m_av_clock->OMXIsPaused())
        {
          CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
          m_av_clock->OMXResume();
        }
      }
      else if (m_Pause || audio_fifo_low || video_fifo_low)
      {
        if (!m_av_clock->OMXIsPaused())
        {
          if (!m_Pause)
            m_threshold = std::min(2.0f*m_threshold, 16.0f);
          CLog::Log(LOGDEBUG, "Pause %.2f,%.2f (%d,%d,%d,%d) %.2f\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_threshold);
          m_av_clock->OMXPause();
        }
      }
    }
    // END OF BUFFERS HANDLING SECTION
    // --------------------------------------------------------------------------------

    // LAST STUFF IN MAIN LOOP:
    // -   -   -   -  -   -   -   - 

		// "RESET" CLOCK IF NOT "SENT STARTED"...    
    if (!sentStarted)
    {
      CLog::Log(LOGDEBUG, "COMXPlayer::HandleMessages - player started RESET");
      m_av_clock->OMXReset(m_has_video, m_has_audio);
      sentStarted = true;
    }

		// IF THERE'S NOT A PACKET THEN READ ONE...?
    if(!m_omx_pkt)
      m_omx_pkt = m_omx_reader.Read();

		// IF THERE'S A PACKET THEN DON'T SEND END OF STREAM...
    if(m_omx_pkt)
      m_send_eos = false;

		// IF WE'RE AT END OF FILE AND NO PACKET IS READ...
    if(m_omx_reader.IsEof() && !m_omx_pkt)
    {
    	// WAIT FOR ANY DATA TO FINISH PLAYING OUT...
      // demuxer EOF, but may have not played out data yet
      if ( (m_has_video && m_player_video.GetCached()) ||
           (m_has_audio && m_player_audio.GetCached()) )
      {
        OMXClock::OMXSleep(sleepDelayMsec);
        continue;
      }
      
      // SEND "END OF STREAM"
      // I THINK THIS FLAG SHOULD BE NAMED "SENT" END OF STREAM (NOT "SEND")???...
      if (!m_send_eos && m_has_video)
        m_player_video.SubmitEOS();
      if (!m_send_eos && m_has_audio)
        m_player_audio.SubmitEOS();
      m_send_eos = true;
      
      // WAIT FOR END OF STREAM TO REGISTER/COMPLETE?...
      if ( (m_has_video && !m_player_video.IsEOS()) ||
           (m_has_audio && !m_player_audio.IsEOS()) )
      {
        OMXClock::OMXSleep(sleepDelayMsec);
        continue;
      }

			//*****************************************************************************************************
			//*****************************************************************************************************
			// IF LOOPING IS ENABLED THEN LOOP...
      if (m_loop)
      {
        m_incr = m_loop_from - (m_av_clock->OMXMediaTime() ? m_av_clock->OMXMediaTime() / DVD_TIME_BASE : last_seek_pos);
        continue;
      }
			//*****************************************************************************************************
			//*****************************************************************************************************
			
			// LOOP NOT SET SO BREAK OUT OF MAIN LOOP...------------------>>>
      break;
    }

		// ADD THE NEXT PACKET TO THE VIDEO, AUDIO, AND SUBTITLE COMPONENTS...
    if(m_has_video && m_omx_pkt && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index))
    {
			// TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/FAST FORWARD (NOT NORMAL PLAY)
      if (TRICKPLAY(m_av_clock->OMXPlaySpeed())) // ADD VIDEO PACKET
      {
         m_packet_after_seek = true;
      }
      if(m_player_video.AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(sleepDelayMsec); // IF FAILED TO ADD PACKET THEN SLEEP 
    }
		// TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/FAST FORWARD (NOT NORMAL PLAY)
    else if(m_has_audio && m_omx_pkt && !TRICKPLAY(m_av_clock->OMXPlaySpeed()) && m_omx_pkt->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if(m_player_audio.AddPacket(m_omx_pkt)) // ADD AUDIO PACKET
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(sleepDelayMsec); // IF FAILED TO ADD PACKET THEN SLEEP 
    }
		// TRICKPLAY IS A MACRO THAT RETURNS TRUE IF IN SLOWMO/FAST FORWARD (NOT NORMAL PLAY)
    else if(m_has_subtitle && m_omx_pkt && !TRICKPLAY(m_av_clock->OMXPlaySpeed()) &&
            m_omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
      auto result = m_player_subtitles.AddPacket(m_omx_pkt,
                      m_omx_reader.GetRelativeIndex(m_omx_pkt->stream_index)); // ADD SUBTITLE PACKET
      if (result)
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(sleepDelayMsec);
    }
    else
    {
      if(m_omx_pkt)
      {
        m_omx_reader.FreePacket(m_omx_pkt); // UNCLEAR WHAT SITUATION CAUSES PROGRAM TO END UP HERE...???
        m_omx_pkt = NULL;
      }
      else
        OMXClock::OMXSleep(sleepDelayMsec);
    }
    
  } // WHILE

// *** END OF MAIN LOOP ***
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark EXIT PLAYER  ~  ~  ~  ~  ~  ~  ~  ~  

//------------------------------------------------------
// EXIT PLAYER / END OF PROGRAM...
do_exit:

  if (m_stats)
    printf("\n");

	// PRINT OUT STOPPED TIME IF STOPPED COMMAND ISSUED...
  if (m_stop)
  {
    unsigned t = (unsigned)(m_av_clock->OMXMediaTime()*1e-6);
    printf("(OMXPLAYER) Stopped at: %02d:%02d:%02d\n", (t/3600), (t/60)%60, t%60);
  }

  if (m_NativeDeinterlace)
  {
    char response[80];
    vc_gencmd(response, sizeof response, "hvs_update_fields %d", 0);
  }
  
  if(m_has_video && m_refresh && tv_state.display.hdmi.group && tv_state.display.hdmi.mode)
  {
    m_BcmHost.vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)tv_state.display.hdmi.group, tv_state.display.hdmi.mode);
  }
 
	// STOP/CLOSE EVERYTHING...
  m_av_clock->OMXStop();
  m_av_clock->OMXStateIdle();

  m_player_subtitles.Close();
  m_player_video.Close();
  m_player_audio.Close();
  if (NULL != m_keyboard)
  {
    m_keyboard->Close();
  }

	// FREE THE PACKET IF THERE IS ONE...
  if(m_omx_pkt)
  {
    m_omx_reader.FreePacket(m_omx_pkt);
    m_omx_pkt = NULL;
  }

  m_omx_reader.Close();

  m_av_clock->OMXDeinitialize();
  if (m_av_clock)
    delete m_av_clock;

  vc_tv_show_info(0);

  g_OMX.Deinitialize();
  g_RBP.Deinitialize();

  //printf("have a nice day ;)\n");

  // exit status success on playback end
  if (m_send_eos)
    return EXIT_SUCCESS;
  // exit status OMXPlayer defined value on user quit
  if (m_stop)
    return 3;
  // exit status failure on other cases
  return EXIT_FAILURE;
}
// END OF FILE
//==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*==*

