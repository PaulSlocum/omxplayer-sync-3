/*
 *      Copyright (C) 2005-2008 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#elif defined(_WIN32)
#include "system.h"
#endif

#include "OMXPlayerVideo.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "linux/XMemUtils.h"

////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTOR
OMXPlayerVideo::OMXPlayerVideo()
{
  m_open          = false;
  m_stream_id     = -1;
  m_pStream       = NULL;
  m_av_clock      = NULL;
  m_decoder       = NULL;
  m_fps           = 25.0f;
  m_flush         = false;
  m_flush_requested = false;
  m_cached_size   = 0;
  m_iVideoDelay   = 0;
  m_iCurrentPts   = 0;

  pthread_cond_init(&m_packet_cond, NULL);
  pthread_cond_init(&m_picture_cond, NULL);
  pthread_mutex_init(&m_lock, NULL);
  pthread_mutex_init(&m_lock_decoder, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////
// DESTRUCTOR
OMXPlayerVideo::~OMXPlayerVideo()
{
  Close();

  pthread_cond_destroy(&m_packet_cond);
  pthread_cond_destroy(&m_picture_cond);
  pthread_mutex_destroy(&m_lock);
  pthread_mutex_destroy(&m_lock_decoder);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::Lock()
{
  if(m_config.use_thread)
    pthread_mutex_lock(&m_lock);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::UnLock()
{
  if(m_config.use_thread)
    pthread_mutex_unlock(&m_lock);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::LockDecoder()
{
  if(m_config.use_thread)
    pthread_mutex_lock(&m_lock_decoder);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::UnLockDecoder()
{
  if(m_config.use_thread)
    pthread_mutex_unlock(&m_lock_decoder);
}

////////////////////////////////////////////////////////////////////////////////////////
// SETS UP DLL (?) AND THEN CALLS OpenDecoder() WHICH SETS UP m_decoder OBJECT THAT 
// CONTAINS COMPONENT AND TUNNEL CHAIN FOR VIDEO DECODING AND RENDERING.
bool OMXPlayerVideo::Open(OMXClock *av_clock, const OMXVideoConfig &config)
{
  if (!m_dllAvUtil.Load() || !m_dllAvCodec.Load() || !m_dllAvFormat.Load() || !av_clock)
    return false;
  
  if(ThreadHandle())
    Close();

  m_dllAvFormat.av_register_all();

  m_config      = config;
  m_av_clock    = av_clock;
  m_fps         = 25.0f;
  m_frametime   = 0;
  m_iCurrentPts = DVD_NOPTS_VALUE;
  m_bAbort      = false;
  m_flush       = false;
  m_cached_size = 0;
  m_iVideoDelay = 0;

  if(!OpenDecoder())
  {
    Close();
    return false;
  }

  if(m_config.use_thread)
    Create();

  m_open        = true;

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////
bool OMXPlayerVideo::Reset()
{
  // Quick reset of internal state back to a default that is ready to play from
  // the start or a new position.  This replaces a combination of Close and then
  // Open calls but does away with the DLL unloading/loading, decoder reset, and
  // thread reset.
  Flush();   
  m_stream_id         = -1;
  m_pStream           = NULL;
  m_iCurrentPts       = DVD_NOPTS_VALUE;
  m_frametime         = 0;
  m_bAbort            = false;
  m_flush             = false;
  m_flush_requested   = false;
  m_cached_size       = 0;
  m_iVideoDelay       = 0;

  // Keep consistency with old Close/Open logic by continuing to return a bool
  // with the success/failure of this call.  Although little can go wrong
  // setting some variables, in the future this could indicate success/failure
  // of the reset.  For now just return success (true).
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////
bool OMXPlayerVideo::Close()
{
  m_bAbort  = true;

  Flush();

  if(ThreadHandle())
  {
    Lock();
    pthread_cond_broadcast(&m_packet_cond);
    UnLock();

    StopThread();
  }

  CloseDecoder();

  m_dllAvUtil.Unload();
  m_dllAvCodec.Unload();
  m_dllAvFormat.Unload();

  m_open          = false;
  m_stream_id     = -1;
  m_iCurrentPts   = DVD_NOPTS_VALUE;
  m_pStream       = NULL;

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::SetAlpha(int alpha)
{
  m_decoder->SetAlpha(alpha);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::SetVideoRect(const CRect& SrcRect, const CRect& DestRect)
{
  m_decoder->SetVideoRect(SrcRect, DestRect);
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::SetVideoRect(int aspectMode)
{
  m_decoder->SetVideoRect(aspectMode);
}

////////////////////////////////////////////////////////////////////////////////////////
// DECODES A PACKET USING THE COMXVideo OBJECT'S OMX COMPONENT CHAIN.
bool OMXPlayerVideo::Decode(OMXPacket *pkt)
{
  if(!pkt)
    return false;

  double dts = pkt->dts;
  double pts = pkt->pts;

  if (dts != DVD_NOPTS_VALUE)
    dts += m_iVideoDelay;

  if (pts != DVD_NOPTS_VALUE)
    pts += m_iVideoDelay;

  if(pts != DVD_NOPTS_VALUE)
    m_iCurrentPts = pts;

  // WAIT UNTIL DECODER HAS ENOUGH FREE SPACE FOR PACKET...
  while((int) m_decoder->GetFreeSpace() < pkt->size)
  {
    OMXClock::OMXSleep(10);
    if(m_flush_requested) return true; // ABORT IF FLUSH REQUESTED ---------->
  }

  CLog::Log(LOGINFO, "CDVDPlayerVideo::Decode dts:%.0f pts:%.0f cur:%.0f, size:%d", pkt->dts, pkt->pts, m_iCurrentPts, pkt->size);
  
  // ADD PACKET TO DECODER CHAIN
  m_decoder->Decode(pkt->data, pkt->size, dts, pts);
  return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// ENDLESS THREADED LOOP THAT FEEDS PACKET TO THE Decode() METHOD ABOVE.
// THIS FUNCTION IS CALLED FROM THE SUPERCLASS OMXThread IN A THREAD...  
void OMXPlayerVideo::Process()
{
  OMXPacket *omx_pkt = NULL;

  // ENDLESS LOOP...
  while(true)
  {
    Lock();
    if(!(m_bStop || m_bAbort) && m_packets.empty())
      pthread_cond_wait(&m_packet_cond, &m_lock);

    if (m_bStop || m_bAbort)
    {
      UnLock();
      break;
    }

    // IF FLUSH SIGNAL...
    if(m_flush && omx_pkt)
    {
      // FLUSH
      OMXReader::FreePacket(omx_pkt);
      omx_pkt = NULL;
      m_flush = false;
    }
    else if(!omx_pkt && !m_packets.empty()) // ELSE - IF NO PACKET...
    {
      // GET THE FIRST PACKET FROM THE QUEUE...
      omx_pkt = m_packets.front(); 
      m_cached_size -= omx_pkt->size;
      m_packets.pop_front();
    }
    UnLock();

    LockDecoder();
    // IF FLUSH SIGNAL...
    if(m_flush && omx_pkt)
    {
      // FLUSH...
      OMXReader::FreePacket(omx_pkt);
      omx_pkt = NULL;
      m_flush = false;
    }
    else if(omx_pkt && Decode(omx_pkt)) // <-- ELSE - ** DECODE PACKET **
    {
      // FREE DECODED PACKET
      OMXReader::FreePacket(omx_pkt);
      omx_pkt = NULL;
    }
    UnLockDecoder();
    
  } // END OF WHILE ______/\_

  if(omx_pkt)
    OMXReader::FreePacket(omx_pkt);
}

////////////////////////////////////////////////////////////////////////////////////////
// FLUSH PACKET QUEUE AND Reset() DECODER CHAIN.
void OMXPlayerVideo::Flush()
{
  m_flush_requested = true;
  Lock();
  LockDecoder();
  m_flush_requested = false;
  m_flush = true;
  while (!m_packets.empty())
  {
    OMXPacket *pkt = m_packets.front(); 
    m_packets.pop_front();
    OMXReader::FreePacket(pkt);
  }
  m_iCurrentPts = DVD_NOPTS_VALUE;
  m_cached_size = 0;
  if(m_decoder)
    m_decoder->Reset();
  UnLockDecoder();
  UnLock();
}

////////////////////////////////////////////////////////////////////////////////////////
// ADD PACKET TO THE PACKET QUEUE.
bool OMXPlayerVideo::AddPacket(OMXPacket *pkt)
{
  bool ret = false;

  if(!pkt)
    return ret;

  if(m_bStop || m_bAbort)
    return ret;

  if((m_cached_size + pkt->size) < m_config.queue_size * 1024 * 1024)
  {
    Lock();
    m_cached_size += pkt->size;
    m_packets.push_back(pkt);
    UnLock();
    ret = true;
    pthread_cond_broadcast(&m_packet_cond);
  }

  return ret;
}


////////////////////////////////////////////////////////////////////////////////////////
// CREATES A NEW COMXVideo OBJECT WHICH CONTAINS THE CHAIN OF OMX COMPONENTS AND 
// TUNNELS FOR DECODING AND RENDERING.
bool OMXPlayerVideo::OpenDecoder()
{
  if (m_config.hints.fpsrate && m_config.hints.fpsscale)
    m_fps = DVD_TIME_BASE / OMXReader::NormalizeFrameduration((double)DVD_TIME_BASE * m_config.hints.fpsscale / m_config.hints.fpsrate);
  else
    m_fps = 25;

  if( m_fps > 100 || m_fps < 5 )
  {
    printf("Invalid framerate %d, using forced 25fps and just trust timestamps\n", (int)m_fps);
    m_fps = 25;
  }

  m_frametime = (double)DVD_TIME_BASE / m_fps;

  // CREATE NEW COMXVideo OBJECT...
  m_decoder = new COMXVideo();
  if(!m_decoder->Open(m_av_clock, m_config))
  {
    // FAILED TO OPEN DECODER
    CloseDecoder();
    return false;
  }
  else
  { 
    //=======================================================================================================
    // DEBUG OUTPUT SHOWING THE NEWLY CREATED VIDEO DECODER
    /*printf("Video codec %s width %d height %d profile %d fps %f\n",
        m_decoder->GetDecoderName().c_str() , m_config.hints.width, m_config.hints.height, m_config.hints.profile, m_fps); //*/
    //=======================================================================================================
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////
bool OMXPlayerVideo::CloseDecoder()
{
  if(m_decoder)
    delete m_decoder;
  m_decoder   = NULL;
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////
int  OMXPlayerVideo::GetDecoderBufferSize()
{
  if(m_decoder)
    return m_decoder->GetInputBufferSize();
  else
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
int  OMXPlayerVideo::GetDecoderFreeSpace()
{
  if(m_decoder)
    return m_decoder->GetFreeSpace();
  else
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
void OMXPlayerVideo::SubmitEOS()
{
  if(m_decoder)
    m_decoder->SubmitEOS();
}

////////////////////////////////////////////////////////////////////////////////////////
bool OMXPlayerVideo::IsEOS()
{
  if(!m_decoder)
    return false;
  return m_packets.empty() && (!m_decoder || m_decoder->IsEOS());
}

