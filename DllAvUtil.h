#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
//***************************************************************************************
//
// HEADER FOR FFMPEG/LIBAVFORMAT DLL
//
//***************************************************************************************

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#endif
#include "DynamicDll.h"
#include "utils/log.h"

#ifndef __GNUC__
#pragma warning(push)
#pragma warning(disable:4244)
#endif

extern "C" {
#if (defined USE_EXTERNAL_FFMPEG)
  #if (defined HAVE_LIBAVUTIL_AVUTIL_H)
    #include <libavutil/avutil.h>
    #include <libavutil/crc.h>
    #include <libavutil/fifo.h>
    // for LIBAVCODEC_VERSION_INT:
    #include <libavcodec/avcodec.h>
  #elif (defined HAVE_FFMPEG_AVUTIL_H)
    #include <ffmpeg/avutil.h>
    #include <ffmpeg/crc.h>
    #include <ffmpeg/fifo.h>
    // for LIBAVCODEC_VERSION_INT:
    #include <ffmpeg/avcodec.h>
  #endif
  #if defined(HAVE_LIBAVUTIL_OPT_H)
    #include <libavutil/opt.h>
  #elif defined(HAVE_LIBAVCODEC_AVCODEC_H)
    #include <libavcodec/opt.h>
  #else
    #include <ffmpeg/opt.h>
  #endif
  #if defined(HAVE_LIBAVUTIL_MEM_H)
    #include <libavutil/mem.h>
  #else
    #include <ffmpeg/mem.h>
  #endif
  #if (defined HAVE_LIBAVUTIL_MATHEMATICS_H)
    #include <libavutil/mathematics.h>
  #endif
#else
  #include "libavutil/avutil.h"
  #include "libavutil/audioconvert.h"
  #include "libavutil/crc.h"
  #include "libavutil/opt.h"
  #include "libavutil/mem.h"
  #include "libavutil/fifo.h"
  #include "libavutil/samplefmt.h"
#endif
}

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52,29,100)
#define AVFRAME_IN_LAVU
#endif

#ifndef __GNUC__
#pragma warning(pop)
#endif

// calback used for logging
//void ff_avutil_log(void* ptr, int level, const char* format, va_list va);

class DllAvUtilInterface
{
public:
  virtual ~DllAvUtilInterface() {}
  virtual void av_log_set_callback(void (*)(void*, int, const char*, va_list))=0;
  virtual void *av_malloc(unsigned int size)=0;
  virtual void *av_mallocz(unsigned int size)=0;
  virtual void *av_realloc(void *ptr, unsigned int size)=0;
  virtual void av_free(void *ptr)=0;
  virtual void av_freep(void *ptr)=0;
  virtual int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding)=0;
  virtual int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq)=0;
  virtual int av_crc_init(AVCRC *ctx, int le, int bits, uint32_t poly, int ctx_size)=0;
  virtual const AVCRC* av_crc_get_table(AVCRCId crc_id)=0;
  virtual uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length)=0;
  virtual int av_opt_set(void *obj, const char *name, const char *val, int search_flags)=0;
  virtual int av_opt_set_double(void *obj, const char *name, double val, int search_flags)=0;
  virtual int av_opt_set_int(void *obj, const char *name, int64_t val, int search_flags)=0;
  virtual AVFifoBuffer *av_fifo_alloc(unsigned int size) = 0;
  virtual void av_fifo_free(AVFifoBuffer *f) = 0;
  virtual void av_fifo_reset(AVFifoBuffer *f) = 0;
  virtual int av_fifo_size(AVFifoBuffer *f) = 0;
  virtual int av_fifo_generic_read(AVFifoBuffer *f, void *dest, int buf_size, void (*func)(void*, void*, int)) = 0;
  virtual int av_fifo_generic_write(AVFifoBuffer *f, void *src, int size, int (*func)(void*, void*, int)) = 0;
  virtual char *av_strdup(const char *s)=0;
  virtual int av_get_bytes_per_sample(enum AVSampleFormat p1) = 0;
  virtual AVDictionaryEntry *av_dict_get(AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags) = 0;
  virtual int av_dict_set(AVDictionary **pm, const char *key, const char *value, int flags)=0;
  virtual int av_dict_parse_string(AVDictionary **pm, const char *str, const char *key_val_sep, const char *pairs_sep, int flags)=0;
  virtual void av_dict_free(AVDictionary **pm) = 0;
  virtual int av_samples_get_buffer_size (int *linesize, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align) = 0;
  virtual int64_t av_get_default_channel_layout(int nb_channels)=0;
  virtual void av_log_set_level(int level) = 0;
  virtual int av_samples_alloc(uint8_t **audio_data, int *linesize, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align) = 0;
  virtual int av_sample_fmt_is_planar(enum AVSampleFormat sample_fmt) = 0;
  virtual int av_get_channel_layout_channel_index (uint64_t channel_layout, uint64_t channel) = 0;
  virtual int av_samples_fill_arrays(uint8_t **audio_data, int *linesize, const uint8_t *buf, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align) = 0;
  virtual int av_samples_copy(uint8_t **dst, uint8_t *const *src, int dst_offset, int src_offset, int nb_samples, int nb_channels, enum AVSampleFormat sample_fmt) = 0;
#if defined(AVFRAME_IN_LAVU)
  virtual void av_frame_free(AVFrame **frame)=0;
  virtual AVFrame *av_frame_alloc(void)=0;
  virtual void av_frame_unref(AVFrame *frame)=0;
  virtual void av_frame_move_ref(AVFrame *dst, AVFrame *src)=0;
#endif
};

#if defined (USE_EXTERNAL_FFMPEG) || (defined TARGET_DARWIN)
// Use direct layer
class DllAvUtilBase : public DllDynamic, DllAvUtilInterface
{
public:

  virtual ~DllAvUtilBase() {}
   virtual void av_log_set_callback(void (*foo)(void*, int, const char*, va_list)) { ::av_log_set_callback(foo); }
   virtual void *av_malloc(unsigned int size) { return ::av_malloc(size); }
   virtual void *av_mallocz(unsigned int size) { return ::av_mallocz(size); }
   virtual void *av_realloc(void *ptr, unsigned int size) { return ::av_realloc(ptr, size); }
   virtual void av_free(void *ptr) { ::av_free(ptr); }
   virtual void av_freep(void *ptr) { ::av_freep(ptr); }
   virtual int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding d) { return ::av_rescale_rnd(a, b, c, d); }
   virtual int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq) { return ::av_rescale_q(a, bq, cq); }
   virtual int av_crc_init(AVCRC *ctx, int le, int bits, uint32_t poly, int ctx_size) { return ::av_crc_init(ctx, le, bits, poly, ctx_size); }
   virtual const AVCRC* av_crc_get_table(AVCRCId crc_id) { return ::av_crc_get_table(crc_id); }
   virtual uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length) { return ::av_crc(ctx, crc, buffer, length); }
   virtual int av_opt_set(void *obj, const char *name, const char *val, int search_flags) { return ::av_opt_set(obj, name, val, search_flags); }
   virtual int av_opt_set_double(void *obj, const char *name, double val, int search_flags) { return ::av_opt_set_double(obj, name, val, search_flags); }
   virtual int av_opt_set_int(void *obj, const char *name, int64_t val, int search_flags) { return ::av_opt_set_int(obj, name, val, search_flags); }
  virtual AVFifoBuffer *av_fifo_alloc(unsigned int size) {return ::av_fifo_alloc(size); }
  virtual void av_fifo_free(AVFifoBuffer *f) { ::av_fifo_free(f); }
  virtual void av_fifo_reset(AVFifoBuffer *f) { ::av_fifo_reset(f); }
  virtual int av_fifo_size(AVFifoBuffer *f) { return ::av_fifo_size(f); }
  virtual int av_fifo_generic_read(AVFifoBuffer *f, void *dest, int buf_size, void (*func)(void*, void*, int))
    { return ::av_fifo_generic_read(f, dest, buf_size, func); }
  virtual int av_fifo_generic_write(AVFifoBuffer *f, void *src, int size, int (*func)(void*, void*, int))
    { return ::av_fifo_generic_write(f, src, size, func); }
  virtual char *av_strdup(const char *s) { return ::av_strdup(s); }
  virtual int av_get_bytes_per_sample(enum AVSampleFormat p1)
    { return ::av_get_bytes_per_sample(p1); }
  virtual AVDictionaryEntry *av_dict_get(AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags){ return ::av_dict_get(m, key, prev, flags); }
  virtual int av_dict_set(AVDictionary **pm, const char *key, const char *value, int flags) { return ::av_dict_set(pm, key, value, flags); }
  virtual int av_dict_parse_string(AVDictionary **pm, const char *str, const char *key_val_sep, const char *pairs_sep, int flags) { return ::av_dict_parse_string(pm, str, key_val_sep, pairs_sep, flags); }
  virtual void av_dict_free(AVDictionary **pm) { ::av_dict_free(pm); }
  virtual int av_samples_get_buffer_size (int *linesize, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align)
    { return ::av_samples_get_buffer_size(linesize, nb_channels, nb_samples, sample_fmt, align); }
  virtual int64_t av_get_default_channel_layout(int nb_channels) { return ::av_get_default_channel_layout(nb_channels); }
  virtual void av_log_set_level(int level) { ::av_log_set_level(level); };
  virtual int av_samples_alloc(uint8_t **audio_data, int *linesize, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align)
    { return ::av_samples_alloc(audio_data, linesize, nb_channels, nb_samples, sample_fmt, align); }
  virtual int av_sample_fmt_is_planar(enum AVSampleFormat sample_fmt) { return ::av_sample_fmt_is_planar(sample_fmt); }
  virtual int av_get_channel_layout_channel_index (uint64_t channel_layout, uint64_t channel) { return ::av_get_channel_layout_channel_index(channel_layout, channel); }
  virtual int av_samples_fill_arrays(uint8_t **audio_data, int *linesize, const uint8_t *buf, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align)
    { return ::av_samples_fill_arrays(audio_data, linesize, buf, nb_channels, nb_samples, sample_fmt, align); }
  virtual int av_samples_copy(uint8_t **dst, uint8_t *const *src, int dst_offset, int src_offset, int nb_samples, int nb_channels, enum AVSampleFormat sample_fmt)
    { return ::av_samples_copy(dst, src, dst_offset, src_offset, nb_samples, nb_channels, sample_fmt); }
#if defined(AVFRAME_IN_LAVU)
  virtual void av_frame_free(AVFrame **frame) { return ::av_frame_free(frame); }
  virtual AVFrame *av_frame_alloc() { return ::av_frame_alloc(); }
  virtual void av_frame_unref(AVFrame *frame) { return ::av_frame_unref(frame); }
  virtual void av_frame_move_ref(AVFrame *dst, AVFrame *src) { return ::av_frame_move_ref(dst,src); }
#endif

   // DLL faking.
   virtual bool ResolveExports() { return true; }
   virtual bool Load() {
     CLog::Log(LOGDEBUG, "DllAvUtilBase: Using libavutil system library");
     return true;
   }
   virtual void Unload() {}
};

#else

class DllAvUtilBase : public DllDynamic, DllAvUtilInterface
{
  DECLARE_DLL_WRAPPER(DllAvUtilBase, DLL_PATH_LIBAVUTIL)

  LOAD_SYMBOLS()

  DEFINE_METHOD1(void, av_log_set_callback, (void (*p1)(void*, int, const char*, va_list)))
  DEFINE_METHOD1(void*, av_malloc, (unsigned int p1))
  DEFINE_METHOD1(void*, av_mallocz, (unsigned int p1))
  DEFINE_METHOD2(void*, av_realloc, (void *p1, unsigned int p2))
  DEFINE_METHOD1(void, av_free, (void *p1))
  DEFINE_METHOD1(void, av_freep, (void *p1))
  DEFINE_METHOD4(int64_t, av_rescale_rnd, (int64_t p1, int64_t p2, int64_t p3, enum AVRounding p4));
  DEFINE_METHOD3(int64_t, av_rescale_q, (int64_t p1, AVRational p2, AVRational p3));
  DEFINE_METHOD1(const AVCRC*, av_crc_get_table, (AVCRCId p1))
  DEFINE_METHOD5(int, av_crc_init, (AVCRC *p1, int p2, int p3, uint32_t p4, int p5));
  DEFINE_METHOD4(uint32_t, av_crc, (const AVCRC *p1, uint32_t p2, const uint8_t *p3, size_t p4));
  DEFINE_METHOD4(int, av_opt_set, (void *p1, const char *p2, const char *p3, int p4));
  DEFINE_METHOD4(int, av_opt_set_double, (void *p1, const char *p2, double p3, int p4))
  DEFINE_METHOD4(int, av_opt_set_int, (void *p1, const char *p2, int64_t p3, int p4))
  DEFINE_METHOD1(AVFifoBuffer*, av_fifo_alloc, (unsigned int p1))
  DEFINE_METHOD1(void, av_fifo_free, (AVFifoBuffer *p1))
  DEFINE_METHOD1(void, av_fifo_reset, (AVFifoBuffer *p1))
  DEFINE_METHOD1(int, av_fifo_size, (AVFifoBuffer *p1))
  DEFINE_METHOD4(int, av_fifo_generic_read, (AVFifoBuffer *p1, void *p2, int p3, void (*p4)(void*, void*, int)))
  DEFINE_METHOD4(int, av_fifo_generic_write, (AVFifoBuffer *p1, void *p2, int p3, int (*p4)(void*, void*, int)))
  DEFINE_METHOD1(char*, av_strdup, (const char *p1))
  DEFINE_METHOD1(int, av_get_bytes_per_sample, (enum AVSampleFormat p1))
  DEFINE_METHOD4(AVDictionaryEntry *, av_dict_get, (AVDictionary *p1, const char *p2, const AVDictionaryEntry *p3, int p4))
  DEFINE_METHOD4(int, av_dict_set, (AVDictionary **p1, const char *p2, const char *p3, int p4));
  DEFINE_METHOD5(int, av_dict_parse_string, (AVDictionary **p1, const char *p2, const char *p3, const char *p4, int p5));
  DEFINE_METHOD1(void, av_dict_free, (AVDictionary **p1));
  DEFINE_METHOD5(int, av_samples_get_buffer_size, (int *p1, int p2, int p3, enum AVSampleFormat p4, int p5))
  DEFINE_METHOD1(int64_t, av_get_default_channel_layout, (int p1))
  DEFINE_METHOD1(void, av_log_set_level, (int p1))
  DEFINE_METHOD6(int, av_samples_alloc, (uint8_t **p1, int *p2, int p3, int p4, enum AVSampleFormat p5, int p6))
  DEFINE_METHOD1(int, av_sample_fmt_is_planar, (enum AVSampleFormat p1))
  DEFINE_METHOD2(int, av_get_channel_layout_channel_index, (uint64_t p1, uint64_t p2))
  DEFINE_METHOD7(int, av_samples_fill_arrays, (uint8_t **p1, int *p2, const uint8_t *p3, int p4, int p5, enum AVSampleFormat p6, int p7))
  DEFINE_METHOD7(int, av_samples_copy, (uint8_t **p1, uint8_t *const *p2, int p3, int p4, int p5, int p6, enum AVSampleFormat p7))
#if defined(AVFRAME_IN_LAVU)
  DEFINE_METHOD1(void, av_frame_free, (AVFrame **p1))
  DEFINE_METHOD0(AVFrame *, av_frame_alloc)
  DEFINE_METHOD1(void, av_frame_unref, (AVFrame *p1))
  DEFINE_METHOD2(void, av_frame_move_ref, (AVFrame *p1, AVFrame* p2))
#endif

  public:
  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(av_log_set_callback)
    RESOLVE_METHOD(av_malloc)
    RESOLVE_METHOD(av_mallocz)
    RESOLVE_METHOD(av_realloc)
    RESOLVE_METHOD(av_free)
    RESOLVE_METHOD(av_freep)
    RESOLVE_METHOD(av_rescale_rnd)
    RESOLVE_METHOD(av_rescale_q)
    RESOLVE_METHOD(av_crc_init)
    RESOLVE_METHOD(av_crc_get_table)
    RESOLVE_METHOD(av_crc)
    RESOLVE_METHOD(av_opt_set)
    RESOLVE_METHOD(av_opt_set_double)
    RESOLVE_METHOD(av_opt_set_int)
    RESOLVE_METHOD(av_fifo_alloc)
    RESOLVE_METHOD(av_fifo_free)
    RESOLVE_METHOD(av_fifo_reset)
    RESOLVE_METHOD(av_fifo_size)
    RESOLVE_METHOD(av_fifo_generic_read)
    RESOLVE_METHOD(av_fifo_generic_write)
    RESOLVE_METHOD(av_strdup)
    RESOLVE_METHOD(av_get_bytes_per_sample)
    RESOLVE_METHOD(av_dict_get)
    RESOLVE_METHOD(av_dict_set)
    RESOLVE_METHOD(av_dict_parse_string)
    RESOLVE_METHOD(av_dict_free)
    RESOLVE_METHOD(av_samples_get_buffer_size)
    RESOLVE_METHOD(av_get_default_channel_layout)
    RESOLVE_METHOD(av_log_set_level)
    RESOLVE_METHOD(av_samples_alloc)
    RESOLVE_METHOD(av_sample_fmt_is_planar)
    RESOLVE_METHOD(av_get_channel_layout_channel_index)
    RESOLVE_METHOD(av_samples_fill_arrays)
    RESOLVE_METHOD(av_samples_copy)
#if defined(AVFRAME_IN_LAVU)
    RESOLVE_METHOD(av_frame_free)
    RESOLVE_METHOD(av_frame_alloc)
    RESOLVE_METHOD(av_frame_unref)
    RESOLVE_METHOD(av_frame_move_ref)
#endif
  END_METHOD_RESOLVE()
};

#endif

class DllAvUtil : public DllAvUtilBase
{
public:
  virtual bool Load()
  {
    if( DllAvUtilBase::Load() )
    {
      //DllAvUtilBase::av_log_set_callback(ff_avutil_log);
      return true;
    }
    return false;
  }
};
