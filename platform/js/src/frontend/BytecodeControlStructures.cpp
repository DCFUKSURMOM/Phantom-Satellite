/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeControlStructures.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"

using namespace js;
using namespace js::frontend;

NestableControl::NestableControl(BytecodeEmitter* bce, StatementKind kind)
  : Nestable<NestableControl>(&bce->innermostNestableControl),
    kind_(kind),
    emitterScope_(bce->innermostEmitterScopeNoCheck())
{}

BreakableControl::BreakableControl(BytecodeEmitter* bce, StatementKind kind)
  : NestableControl(bce, kind)
{
    MOZ_ASSERT(is<BreakableControl>());
}

bool
BreakableControl::patchBreaks(BytecodeEmitter* bce)
{
    return bce->emitJumpTargetAndPatch(breaks);
}

LabelControl::LabelControl(BytecodeEmitter* bce, JSAtom* label, ptrdiff_t startOffset)
  : BreakableControl(bce, StatementKind::Label),
    label_(bce->cx, label),
    startOffset_(startOffset)
{}

LoopControl::LoopControl(BytecodeEmitter* bce, StatementKind loopKind)
  : BreakableControl(bce, loopKind),
    tdzCache_(bce),
    continueTarget({ -1 })
{
    MOZ_ASSERT(is<LoopControl>());

    LoopControl* enclosingLoop = findNearest<LoopControl>(enclosing());

    stackDepth_ = bce->stackDepth;
    loopDepth_ = enclosingLoop ? enclosingLoop->loopDepth_ + 1 : 1;

    int loopSlots;
    if (loopKind == StatementKind::Spread || loopKind == StatementKind::ForOfLoop)
        loopSlots = 3;
    else if (loopKind == StatementKind::ForInLoop)
        loopSlots = 2;
    else
        loopSlots = 0;

    MOZ_ASSERT(loopSlots <= stackDepth_);

    if (enclosingLoop) {
        canIonOsr_ = (enclosingLoop->canIonOsr_ &&
                      stackDepth_ == enclosingLoop->stackDepth_ + loopSlots);
    } else {
        canIonOsr_ = stackDepth_ == loopSlots;
    }
}

bool
LoopControl::patchBreaksAndContinues(BytecodeEmitter* bce)
{
    MOZ_ASSERT(continueTarget.offset != -1);
    if (!patchBreaks(bce))
        return false;
    bce->patchJumpsToTarget(continues, continueTarget);
    return true;
}

TryFinallyControl::TryFinallyControl(BytecodeEmitter* bce, StatementKind kind)
  : NestableControl(bce, kind),
    emittingSubroutine_(false)
{
    MOZ_ASSERT(is<TryFinallyControl>());
}
