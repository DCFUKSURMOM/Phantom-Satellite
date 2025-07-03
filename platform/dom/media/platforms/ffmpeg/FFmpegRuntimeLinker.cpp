/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegRuntimeLinker.h"
#include "FFmpegLibWrapper.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"
#include "prlink.h"

namespace mozilla
{

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;
const char* FFmpegRuntimeLinker::sLinkStatusLibraryName = "";

template <int V> class FFmpegDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule> Create(FFmpegLibWrapper*);
};

static FFmpegLibWrapper sLibAV;

static const char* sLibs[] = {
#if defined(XP_DARWIN)
  "libavcodec.60.dylib",
  "libavcodec.59.dylib",
  "libavcodec.58.dylib",
#else
  "libavcodec.so.60",
  "libavcodec.so.59",
  "libavcodec.so.58",
  "libavcodec-ffmpeg.so.58",
#endif
};

/* static */ bool
FFmpegRuntimeLinker::Init()
{
  if (sLinkStatus != LinkStatus_INIT) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  // While going through all possible libs, this status will be updated with a
  // more precise error if possible.
  sLinkStatus = LinkStatus_NOT_FOUND;

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const char* lib = sLibs[i];
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = lib;
    sLibAV.mAVCodecLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
    if (sLibAV.mAVCodecLib) {
      sLibAV.mAVUtilLib = sLibAV.mAVCodecLib;
      switch (sLibAV.Link()) {
        case FFmpegLibWrapper::LinkResult::Success:
          sLinkStatus = LinkStatus_SUCCEEDED;
          sLinkStatusLibraryName = lib;
          return true;
        case FFmpegLibWrapper::LinkResult::NoProvidedLib:
          MOZ_ASSERT_UNREACHABLE("Incorrectly-setup sLibAV");
          break;
        case FFmpegLibWrapper::LinkResult::NoAVCodecVersion:
          if (sLinkStatus > LinkStatus_INVALID_CANDIDATE) {
            sLinkStatus = LinkStatus_INVALID_CANDIDATE;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::UnknownFFMpegVersion:
        case FFmpegLibWrapper::LinkResult::MissingFFMpegFunction:
          if (sLinkStatus > LinkStatus_INVALID_FFMPEG_CANDIDATE) {
            sLinkStatus = LinkStatus_INVALID_FFMPEG_CANDIDATE;
            sLinkStatusLibraryName = lib;
          }
          break;
      }
    }
  }

  FFMPEG_LOG("H264/AAC codecs unsupported without [");
  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    FFMPEG_LOG("%s %s", i ? "," : " ", sLibs[i]);
  }
  FFMPEG_LOG(" ]\n");

  return false;
}

/* static */ already_AddRefed<PlatformDecoderModule>
FFmpegRuntimeLinker::CreateDecoderModule()
{
  if (!Init()) {
    return nullptr;
  }
  RefPtr<PlatformDecoderModule> module;
  switch (sLibAV.mVersion) {
    case 58: module = FFmpegDecoderModule<58>::Create(&sLibAV); break;
    case 59: module = FFmpegDecoderModule<59>::Create(&sLibAV); break;
    case 60: module = FFmpegDecoderModule<60>::Create(&sLibAV); break;
    default: module = nullptr;
  }
  return module.forget();
}

/* static */ const char*
FFmpegRuntimeLinker::LinkStatusString()
{
  switch (sLinkStatus) {
    case LinkStatus_INIT:
      return "Libavcodec not initialized yet";
    case LinkStatus_SUCCEEDED:
      return "Libavcodec linking succeeded";
    case LinkStatus_INVALID_FFMPEG_CANDIDATE:
      return "Invalid FFMpeg libavcodec candidate";
    case LinkStatus_INVALID_CANDIDATE:
      return "Invalid libavcodec candidate";
    case LinkStatus_NOT_FOUND:
      return "Libavcodec not found";
  }
  MOZ_ASSERT_UNREACHABLE("Unknown sLinkStatus value");
  return "?";
}

} // namespace mozilla
