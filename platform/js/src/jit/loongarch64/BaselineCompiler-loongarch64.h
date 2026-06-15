/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_loongarch64_BaselineCompiler_loongarch64_h
#define jit_loongarch64_BaselineCompiler_loongarch64_h

#include "jit/mips-shared/BaselineCompiler-mips-shared.h"

namespace js {
namespace jit {

class BaselineCompilerLOONGARCH64 : public BaselineCompilerMIPSShared
{
  protected:
    BaselineCompilerLOONGARCH64(JSContext* cx, TempAllocator& alloc, JSScript* script);
};

typedef BaselineCompilerLOONGARCH64 BaselineCompilerSpecific;

} // namespace jit
} // namespace js

#endif /* jit_loongarch64_BaselineCompiler_loongarch64_h */
