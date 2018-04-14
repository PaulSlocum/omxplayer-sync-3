/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */


#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#elif defined(_WIN32)
#include "system.h"
#endif

#if defined(HAVE_OMXLIB)

//#include "video/VideoReferenceClock.h"
//#include "settings/Settings.h"

#include "OMXClock.h"

#define OMX_PRE_ROLL 200
#define TP(speed) ((speed) < 0 || (speed) > 4*DVD_PLAYSPEED_NORMAL)


/////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTOR
OMXClock::OMXClock()
{
  m_dllAvFormat.Load();

  m_pause       = false;

  m_omx_speed = DVD_PLAYSPEED_NORMAL;
  m_WaitMask = 0;
  m_eState = OMX_TIME_ClockStateStopped;
  m_eClock = OMX_TIME_RefClockNone;
  m_last_media_time = 0.0f;
  m_last_media_time_read = 0.0f;

  pthread_mutex_init(&m_lock, NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////
// DESTRUCTOR
OMXClock::~OMXClock()
{
  OMXDeinitialize();

  m_dllAvFormat.Unload();
  pthread_mutex_destroy(&m_lock);
}

/////////////////////////////////////////////////////////////////////////////////////////
void OMXClock::Lock()
{
  pthread_mutex_lock(&m_lock);
}

/////////////////////////////////////////////////////////////////////////////////////////
void OMXClock::UnLock()
{
  pthread_mutex_unlock(&m_lock);
}

/////////////////////////////////////////////////////////////////////////////////////////
void OMXClock::OMXSetClockPorts(OMX_TIME_CONFIG_CLOCKSTATETYPE *clock, bool has_video, bool has_audio)
{
  if(m_omx_clock.GetComponent() == NULL)
    return;

  if(!clock)
    return;

  clock->nWaitMask = 0;

  if(has_audio)
  {
    clock->nWaitMask |= OMX_CLOCKPORT0;
  }

  if(has_video)
  {
    clock->nWaitMask |= OMX_CLOCKPORT1;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
bool OMXClock::OMXSetReferenceClock(bool has_audio, bool lock /* = true */)
{
  if(lock)
    Lock();

  bool ret = true;
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE refClock;
  OMX_INIT_STRUCTURE(refClock);

  if(has_audio)
    refClock.eClock = OMX_TIME_RefClockAudio;
  else
    refClock.eClock = OMX_TIME_RefClockVideo;

  if (refClock.eClock != m_eClock)
  {
    CLog::Log(LOGNOTICE, "OMXClock using %s as reference", refClock.eClock == OMX_TIME_RefClockVideo ? "video" : "audio");

    omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeActiveRefClock, &refClock);
    if(omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "OMXClock::OMXSetReferenceClock error setting OMX_IndexConfigTimeActiveRefClock");
      ret = false;
    }
    m_eClock = refClock.eClock;
  }
  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////
// INTIALIZE OMX CLOCK COMPONENT
bool OMXClock::OMXInitialize()
{
  std::string componentName = "";

  m_pause       = false;

  componentName = "OMX.broadcom.clock";
  if(!m_omx_clock.Initialize((const std::string)componentName, OMX_IndexParamOtherInit))
    return false;

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
void OMXClock::OMXDeinitialize()
{
  if(m_omx_clock.GetComponent() == NULL)
    return;

  m_omx_clock.Deinitialize();

  m_omx_speed = DVD_PLAYSPEED_NORMAL;
  m_last_media_time = 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////////////
// SETS OMX CLOCK COMPONENT TO STATE OMX_StateExecuting.
// ACCORDING TO THE OPENMAX DOCS: OMX_StateExecuting = "Component is transferring buffers and is processing data"
bool OMXClock::OMXStateExecute(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(m_omx_clock.GetState() != OMX_StateExecuting)
  {

    OMXStateIdle(false);

    omx_err = m_omx_clock.SetStateForComponent(OMX_StateExecuting); 
    if (omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "OMXClock::StateExecute m_omx_clock.SetStateForComponent\n");
      if(lock)
        UnLock();
      return false;
    }
  }

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OMX_StateIdle = "Component has all resources but has not transferred any buffers or begun processing data."
void OMXClock::OMXStateIdle(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return;

  if(lock)
    Lock();

  if(m_omx_clock.GetState() != OMX_StateIdle)
    m_omx_clock.SetStateForComponent(OMX_StateIdle);

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();
}

/////////////////////////////////////////////////////////////////////////////////////////
// RETURNS A POINTER TO THE CLOCK COMPONENT
COMXCoreComponent *OMXClock::GetOMXClock()
{
  return &m_omx_clock;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool  OMXClock::OMXStop(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  CLog::Log(LOGDEBUG, "OMXClock::OMXStop\n");

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
  OMX_INIT_STRUCTURE(clock);

  clock.eState      = OMX_TIME_ClockStateStopped;
  clock.nOffset     = ToOMXTime(-1000LL * OMX_PRE_ROLL);

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeClockState, &clock);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "OMXClock::Stop error setting OMX_IndexConfigTimeClockState\n");
    if(lock)
      UnLock();
    return false;
  }
  m_eState = clock.eState;

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool OMXClock::OMXStep(int steps /* = 1 */, bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_PARAM_U32TYPE param;
  OMX_INIT_STRUCTURE(param);

  param.nPortIndex = OMX_ALL;
  param.nU32 = steps;

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigSingleStep, &param);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "OMXClock::Error setting OMX_IndexConfigSingleStep\n");
    if(lock)
      UnLock();
    return false;
  }

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  CLog::Log(LOGDEBUG, "OMXClock::Step (%d)", steps);
  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool OMXClock::OMXReset(bool has_video, bool has_audio, bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  if(!OMXSetReferenceClock(has_audio, false))
  {
    if(lock)
      UnLock();
    return false;
  }

  if (m_eState == OMX_TIME_ClockStateStopped)
  {
    OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
    OMX_INIT_STRUCTURE(clock);

    clock.eState    = OMX_TIME_ClockStateWaitingForStartTime;
    clock.nOffset   = ToOMXTime(-1000LL * OMX_PRE_ROLL);

    OMXSetClockPorts(&clock, has_video, has_audio);

    if(clock.nWaitMask)
    {
      OMX_ERRORTYPE omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeClockState, &clock);
      if(omx_err != OMX_ErrorNone)
      {
        CLog::Log(LOGERROR, "OMXClock::OMXReset error setting OMX_IndexConfigTimeClockState\n");
        if(lock)
          UnLock();
        return false;
      }
      CLog::Log(LOGDEBUG, "OMXClock::OMXReset audio / video : %d / %d wait mask %d->%d state : %d->%d\n",
          has_audio, has_video, m_WaitMask, clock.nWaitMask, m_eState, clock.eState);
      if (m_eState != OMX_TIME_ClockStateStopped)
        m_WaitMask = clock.nWaitMask;
      m_eState = clock.eState;
    }
  }

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}


///////////////////////////////////////////////////////////////////////
// *GETS* THE MEDIA TIME (UNLIKE THE OTHER FUNCTION WITH THE SAME NAME THAT *SETS* THE MEDIA TIME)
double OMXClock::OMXMediaTime(bool lock /* = true */)
{
  double pts = 0.0;
  if(m_omx_clock.GetComponent() == NULL)
    return 0;

  double now = GetAbsoluteClock();
  if (now - m_last_media_time_read > DVD_MSEC_TO_TIME(100) || m_last_media_time == 0.0)
  {
    if(lock)
      Lock();

    OMX_ERRORTYPE omx_err = OMX_ErrorNone;

    OMX_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
    OMX_INIT_STRUCTURE(timeStamp);
    timeStamp.nPortIndex = m_omx_clock.GetInputPort();

    omx_err = m_omx_clock.GetConfig(OMX_IndexConfigTimeCurrentMediaTime, &timeStamp);
    if(omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "OMXClock::MediaTime error getting OMX_IndexConfigTimeCurrentMediaTime\n");
      if(lock)
        UnLock();
      return 0;
    }

    pts = FromOMXTime(timeStamp.nTimestamp);
    //CLog::Log(LOGINFO, "OMXClock::MediaTime %.2f (%.2f, %.2f)", pts, m_last_media_time, now - m_last_media_time_read);
    m_last_media_time = pts;
    m_last_media_time_read = now;

    if(lock)
      UnLock();
  }
  else
  {
    double speed = m_pause ? 0.0 : (double)m_omx_speed / DVD_PLAYSPEED_NORMAL;
    pts = m_last_media_time + (now - m_last_media_time_read) * speed;
    //CLog::Log(LOGINFO, "OMXClock::MediaTime cached %.2f (%.2f, %.2f)", pts, m_last_media_time, now - m_last_media_time_read);
  }
  return pts;
}

/////////////////////////////////////////////////////////////////////////////////////////
double OMXClock::OMXClockAdjustment(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return 0;

  if(lock)
    Lock();

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  double pts = 0;

  OMX_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
  OMX_INIT_STRUCTURE(timeStamp);
  timeStamp.nPortIndex = m_omx_clock.GetInputPort();

  omx_err = m_omx_clock.GetConfig(OMX_IndexConfigClockAdjustment, &timeStamp);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "OMXClock::MediaTime error getting OMX_IndexConfigClockAdjustment\n");
    if(lock)
      UnLock();
    return 0;
  }

  pts = (double)FromOMXTime(timeStamp.nTimestamp);
  //CLog::Log(LOGINFO, "OMXClock::ClockAdjustment %.0f %.0f\n", (double)FromOMXTime(timeStamp.nTimestamp), pts);
  if(lock)
    UnLock();

  return pts;
}


/////////////////////////////////////////////////////////////////////////////////////////
// *SETS* THE MEDIA TIME (UNLIKE THE OTHER FUNCTION WITH THE SAME NAME THAT *GETS* THE MEDIA TIME)
// Set the media time, so calls to get media time use the updated value,
// useful after a seek so mediatime is updated immediately (rather than waiting for first decoded packet)
bool OMXClock::OMXMediaTime(double pts, bool lock /* = true*/)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_INDEXTYPE index;
  OMX_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
  OMX_INIT_STRUCTURE(timeStamp);
  timeStamp.nPortIndex = m_omx_clock.GetInputPort();

  if(m_eClock == OMX_TIME_RefClockAudio)
    index = OMX_IndexConfigTimeCurrentAudioReference;
  else
    index = OMX_IndexConfigTimeCurrentVideoReference;

  // CONVERT THE GIVEN TIME DOUBLE TO THE OMX TIME STRUCT (TWO 32BIT INTS IN A STRUCT) 
  timeStamp.nTimestamp = ToOMXTime(pts);

  // SET THE NEW TIME (I THINK?)...
  omx_err = m_omx_clock.SetConfig(index, &timeStamp);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "OMXClock::OMXMediaTime error setting %s", index == OMX_IndexConfigTimeCurrentAudioReference ?
       "OMX_IndexConfigTimeCurrentAudioReference":"OMX_IndexConfigTimeCurrentVideoReference");
    if(lock)
      UnLock();
    return false;
  }

  CLog::Log(LOGDEBUG, "OMXClock::OMXMediaTime set config %s = %.2f", index == OMX_IndexConfigTimeCurrentAudioReference ?
       "OMX_IndexConfigTimeCurrentAudioReference":"OMX_IndexConfigTimeCurrentVideoReference", pts);

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// PAUSE (THIS FUNCTION IS NOT A TOGGLE)
bool OMXClock::OMXPause(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(!m_pause)
  {
    if(lock)
      Lock();

    if (OMXSetSpeed(0, false, true))
      m_pause = true;

    m_last_media_time = 0.0f;
    if(lock)
      UnLock();
  }
  return m_pause == true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// UNPAUSE (THIS FUNCTION IS NOT A TOGGLE)
bool OMXClock::OMXResume(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(m_pause)
  {
    if(lock)
      Lock();

    if (OMXSetSpeed(m_omx_speed, false, true))
      m_pause = false;

    m_last_media_time = 0.0f;
    if(lock)
      UnLock();
  }
  return m_pause == false;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool OMXClock::OMXSetSpeed(int speed, bool lock /* = true */, bool pause_resume /* = false */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  CLog::Log(LOGDEBUG, "OMXClock::OMXSetSpeed(%.2f) pause_resume:%d", (float)speed / (float)DVD_PLAYSPEED_NORMAL, pause_resume);

  if (pause_resume)
  {
    OMX_ERRORTYPE omx_err = OMX_ErrorNone;
    OMX_TIME_CONFIG_SCALETYPE scaleType;
    OMX_INIT_STRUCTURE(scaleType);

    if (TP(speed))
      scaleType.xScale = 0; // for trickplay we just pause, and single step
    else
      scaleType.xScale = (speed << 16) / DVD_PLAYSPEED_NORMAL;
    omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeScale, &scaleType);
    if(omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "OMXClock::OMXSetSpeed error setting OMX_IndexConfigTimeClockState\n");
      if(lock)
        UnLock();
      return false;
    }
  }
  if (!pause_resume)
    m_omx_speed = speed;

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool OMXClock::HDMIClockSync(bool lock /* = true */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(lock)
    Lock();

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
  OMX_INIT_STRUCTURE(latencyTarget);

  latencyTarget.nPortIndex = OMX_ALL;
  latencyTarget.bEnabled = OMX_TRUE;
  latencyTarget.nFilter = 10;
  latencyTarget.nTarget = 0;
  latencyTarget.nShift = 3;
  latencyTarget.nSpeedFactor = -60;
  latencyTarget.nInterFactor = 100;
  latencyTarget.nAdjCap = 100;

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigLatencyTarget, &latencyTarget);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "OMXClock::Speed error setting OMX_IndexConfigLatencyTarget\n");
    if(lock)
      UnLock();
    return false;
  }

  m_last_media_time = 0.0f;
  if(lock)
    UnLock();

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// SLEEP IN MILLISECONDS
void OMXClock::OMXSleep(unsigned int dwMilliSeconds)
{
  struct timespec req;
  req.tv_sec = dwMilliSeconds / 1000;
  req.tv_nsec = (dwMilliSeconds % 1000) * 1000000;

  while ( nanosleep(&req, &req) == -1 && errno == EINTR && (req.tv_nsec > 0 || req.tv_sec > 0));
}

//======================================================================================
// THIS IS MIGHT BE THE ONLY FUNCTION THAT ACTUALLY GETS THE SYSTEM TIME???
// LOOKS LIKE THIS RETURNS THE TIME IN NANOSECONDS...
static int64_t CurrentHostCounter(void)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return( ((int64_t)now.tv_sec * 1000000000L) + now.tv_nsec );
}

/////////////////////////////////////////////////////////////////////////////////////////
// RETURNS MICROSECONDS(?) AS 64 BIT INTEGER...
int64_t OMXClock::GetAbsoluteClock()
{
  return CurrentHostCounter()/1000; // LOOKS LIKE THIS CONVERTS NANOSECONDS TO MICROSECONDS???
}

/////////////////////////////////////////////////////////////////////////////////////////
// RETURNS MICROSECONDS(?) AS DOUBLE...
double OMXClock::GetClock(bool interpolated /*= true*/)
{
  return GetAbsoluteClock();
}
#endif








