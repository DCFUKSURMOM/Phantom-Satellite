/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_

#include "FFmpegLibWrapper.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

// Access the correct location for the channel count, based on ffmpeg version.
template<typename T>
inline int& ChannelCount(T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR <= 59
  return aObject->channels;
#else
  return aObject->ch_layout.nb_channels;
#endif
}

// Access the correct location for the duration, based on ffmpeg version.
template<typename T>
inline int64_t& Duration(T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR < 61
  return aObject->pkt_duration;
#else
  return aObject->duration;
#endif
}

// Access the correct location for the duration, based on ffmpeg version.
template<typename T>
inline const int64_t& Duration(const T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR < 61
  return aObject->pkt_duration;
#else
  return aObject->duration;
#endif
}

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_
