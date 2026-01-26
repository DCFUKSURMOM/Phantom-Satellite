/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_FFVPX_CONFIG_H
#define MOZ_FFVPX_CONFIG_H

#if defined(XP_WIN)
// Avoid conflicts with mozilla-config.h
#if !defined(_MSC_VER)
#undef HAVE_DIRENT_H
#undef HAVE_UNISTD_H
#endif
#if defined(_ARM64_)
#include "config_win64_aarch64.h"
#else
#if defined(HAVE_64BIT_BUILD)
#include "config_win64.h"
#else
#include "config_win32.h"
#endif
#endif
// Adjust configure defines for GCC
#if !defined(_MSC_VER)
#if !defined(HAVE_64BIT_BUILD)
#undef HAVE_MM_EMPTY
#define HAVE_MM_EMPTY 0
#endif
#undef HAVE_LIBC_MSVCRT
#define HAVE_LIBC_MSVCRT 0
#endif
#elif defined(XP_DARWIN)
#if defined(__aarch64__)
#include "config_darwin_aarch64.h"
#elif defined(__ppc__)
#include "config_darwin_ppc.h"
#else
#include "config_darwin64.h"
#endif
#elif defined(XP_UNIX)
#if defined(__aarch64__)
#include "config_unix_aarch64.h"
#elif defined(__alpha) || defined(__alpha__)
#include "config_unix_alpha.h"
#elif defined(__powerpc__)
#include "config_unix_ppc.h"
#elif defined(__sparcv9) || defined(__sparcv9__)
#include "config_unix_sparc64.h"
#elif defined(__loongarch64)
#include "config_unix_loongarch64.h"
#else
#if defined(HAVE_64BIT_BUILD)
#include "config_unix64.h"
#else
#include "config_unix32.h"
#endif
#endif
#endif

#include "config_override.h"

#endif // MOZ_FFVPX_CONFIG_H
