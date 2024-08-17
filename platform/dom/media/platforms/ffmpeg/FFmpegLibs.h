/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibs_h__
#define __FFmpegLibs_h__

extern "C" {
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#ifdef __GNUC__
#pragma GCC visibility pop
#endif
}

#ifdef FFVPX_VERSION
enum { LIBAV_VER = FFVPX_VERSION };
#else
enum { LIBAV_VER = LIBAVCODEC_VERSION_MAJOR };
#endif

#endif // __FFmpegLibs_h__
