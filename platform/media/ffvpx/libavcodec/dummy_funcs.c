/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "avcodec.h"
#include "bsf.h"
#include "bsf_internal.h"

typedef struct VP8DSPContext VP8DSPContext;

int ff_frame_thread_encoder_init(AVCodecContext *avctx, AVDictionary *options) { return 0; }
void ff_frame_thread_encoder_free(AVCodecContext *avctx) {}
int ff_thread_video_encode_frame(AVCodecContext *avctx, AVPacket *pkt, const AVFrame *frame, int *got_packet_ptr) { return 0; }
void ff_vp7dsp_init(VP8DSPContext *c) {}
