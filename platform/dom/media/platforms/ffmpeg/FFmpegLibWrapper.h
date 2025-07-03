/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibWrapper_h__
#define __FFmpegLibWrapper_h__

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "ffvpx/tx.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVDictionary;
struct AVCodecParserContext;
struct PRLibrary;

namespace mozilla
{

struct FFmpegLibWrapper
{
  FFmpegLibWrapper();
  ~FFmpegLibWrapper();

  enum class LinkResult
  {
    Success,
    NoProvidedLib,
    NoAVCodecVersion,
    UnknownFFMpegVersion,
    MissingFFMpegFunction,
  };
  // Examine mAVCodecLib and mAVUtilLib, and attempt to resolve all symbols.
  // Upon failure, the entire object will be reset and any attached libraries
  // will be unlinked.
  LinkResult Link();

  // Reset the wrapper and unlink all attached libraries.
  void Unlink();

  // indicate the version of libavcodec linked to.
  // 0 indicates that the function wasn't initialized with Link().
  int mVersion;

  // libavcodec
  unsigned (*avcodec_version)();
  int (*av_lockmgr_register)(int (*cb)(void** mutex, int op));
  AVCodecContext* (*avcodec_alloc_context3)(const AVCodec* codec);
  int (*avcodec_close)(AVCodecContext* avctx);
  int (*avcodec_decode_audio4)(AVCodecContext* avctx, AVFrame* frame, int* got_frame_ptr, const AVPacket* avpkt);
  int (*avcodec_decode_video2)(AVCodecContext* avctx, AVFrame* picture, int* got_picture_ptr, const AVPacket* avpkt);
  AVCodec* (*avcodec_find_decoder)(int id);
  void (*avcodec_flush_buffers)(AVCodecContext *avctx);
  int (*avcodec_open2)(AVCodecContext *avctx, const AVCodec* codec, AVDictionary** options);
  void (*avcodec_register_all)();
  void (*av_init_packet)(AVPacket* pkt);
  AVCodecParserContext* (*av_parser_init)(int codec_id);
  void (*av_parser_close)(AVCodecParserContext* s);
  int (*av_parser_parse2)(AVCodecParserContext* s, AVCodecContext* avctx, uint8_t** poutbuf, int* poutbuf_size, const uint8_t* buf, int buf_size, int64_t pts, int64_t dts, int64_t pos);
  int (*avcodec_send_packet)(AVCodecContext* avctx, const AVPacket* avpkt);
  int (*avcodec_receive_frame)(AVCodecContext* avctx, AVFrame* frame);

  // libavutil
  void (*av_log_set_level)(int level);
  void*	(*av_malloc)(size_t size);
  void (*av_freep)(void *ptr);
  AVFrame* (*av_frame_alloc)();
  void (*av_frame_free)(AVFrame** frame);
  void (*av_frame_unref)(AVFrame* frame);
  int (*av_frame_get_colorspace)(const AVFrame *frame);
  int (*av_frame_get_color_range)(const AVFrame *frame);

  // Only ever used with ffvpx
  decltype(::av_tx_init)* av_tx_init;
  decltype(::av_tx_uninit)* av_tx_uninit;

  PRLibrary* mAVCodecLib;
  PRLibrary* mAVUtilLib;

private:
};

} // namespace mozilla

#endif // FFmpegLibWrapper
