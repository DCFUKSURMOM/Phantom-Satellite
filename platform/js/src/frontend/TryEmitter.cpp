/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/TryEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

TryEmitter::TryEmitter(BytecodeEmitter* bce, Kind kind,
                       ShouldUseRetVal retValKind,
                       ShouldUseControl controlKind)
  : bce_(bce),
    kind_(kind),
    retValKind_(retValKind),
    depth_(0),
    noteIndex_(0),
    tryStart_(0),
    state_(Start)
{
    if (controlKind == UseControl)
        controlInfo_.emplace(bce_, hasFinally() ? StatementKind::Finally : StatementKind::Try);
    finallyStart_.offset = 0;
}

// Emits JSOP_GOTO to the end of try-catch-finally.
// Used in `yield*`.
bool
TryEmitter::emitJumpOverCatchAndFinally()
{
    if (!bce_->emitJump(JSOP_GOTO, &catchAndFinallyJump_))
        return false;
    return true;
}

bool
TryEmitter::emitTry()
{
    MOZ_ASSERT(state_ == Start);

    // Since an exception can be thrown at any place inside the try block,
    // we need to restore the stack and the scope chain before we transfer
    // the control to the exception handler.
    //
    // For that we store in a try note associated with the catch or
    // finally block the stack depth upon the try entry. The interpreter
    // uses this depth to properly unwind the stack and the scope chain.
    depth_ = bce_->stackDepth;

    // Record the try location, then emit the try block.
    if (!bce_->newSrcNote(SRC_TRY, &noteIndex_))
        return false;
    if (!bce_->emit1(JSOP_TRY))
        return false;
    tryStart_ = bce_->offset();

    state_ = Try;
    return true;
}

bool
TryEmitter::emitTryEnd()
{
    MOZ_ASSERT(state_ == Try);
    MOZ_ASSERT(depth_ == bce_->stackDepth);

    // GOSUB to finally, if present.
    if (hasFinally() && controlInfo_) {
        if (!bce_->emitJump(JSOP_GOSUB, &controlInfo_->gosubs))
            return false;
    }

    // Source note points to the jump at the end of the try block.
    if (!bce_->setSrcNoteOffset(noteIndex_, 0, bce_->offset() - tryStart_ + JSOP_TRY_LENGTH))
        return false;

    // Emit jump over catch and/or finally.
    if (!bce_->emitJump(JSOP_GOTO, &catchAndFinallyJump_))
        return false;

    if (!bce_->emitJumpTarget(&tryEnd_))
        return false;

    return true;
}

bool
TryEmitter::emitCatch()
{
    if (state_ == Try) {
        if (!emitTryEnd())
            return false;
    } else {
        MOZ_ASSERT(state_ == Catch);
        if (!emitCatchEnd(true))
            return false;
    }

    MOZ_ASSERT(bce_->stackDepth == depth_);

    if (retValKind_ == UseRetVal) {
        // Clear the frame's return value that might have been set by the
        // try block:
        //
        //   eval("try { 1; throw 2 } catch(e) {}"); // undefined, not 1
        if (!bce_->emit1(JSOP_UNDEFINED))
            return false;
        if (!bce_->emit1(JSOP_SETRVAL))
            return false;
    }

    state_ = Catch;
    return true;
}


bool
TryEmitter::emitCatchEnd(bool hasNext)
{
    MOZ_ASSERT(state_ == Catch);

    if (!controlInfo_)
        return true;

    // gosub <finally>, if required.
    if (hasFinally()) {
        if (!bce_->emitJump(JSOP_GOSUB, &controlInfo_->gosubs))
            return false;
        MOZ_ASSERT(bce_->stackDepth == depth_);
    }

    // Jump over the remaining catch blocks.  This will get fixed
    // up to jump to after catch/finally.
    if (!bce_->emitJump(JSOP_GOTO, &catchAndFinallyJump_))
        return false;

    // If this catch block had a guard clause, patch the guard jump to
    // come here.
    if (controlInfo_->guardJump.offset != -1) {
        if (!bce_->emitJumpTargetAndPatch(controlInfo_->guardJump))
            return false;
        controlInfo_->guardJump.offset = -1;

        // If this catch block is the last one, rethrow, delegating
        // execution of any finally block to the exception handler.
        if (!hasNext) {
            if (!bce_->emit1(JSOP_EXCEPTION))
                return false;
            if (!bce_->emit1(JSOP_THROW))
                return false;
        }
    }

    return true;
}

bool
TryEmitter::emitFinally(const Maybe<uint32_t>& finallyPos /* = Nothing() */)
{
    // If we are using controlInfo_ (i.e., emitting a syntactic try
    // blocks), we must have specified up front if there will be a finally
    // close. For internal try blocks, like those emitted for yield* and
    // IteratorClose inside for-of loops, we can emitFinally even without
    // specifying up front, since the internal try blocks emit no GOSUBs.
    if (!controlInfo_) {
        if (kind_ == TryCatch)
            kind_ = TryCatchFinally;
    } else {
        MOZ_ASSERT(hasFinally());
    }

    if (state_ == Try) {
        if (!emitTryEnd())
            return false;
    } else {
        MOZ_ASSERT(state_ == Catch);
        if (!emitCatchEnd(false))
            return false;
    }

    MOZ_ASSERT(bce_->stackDepth == depth_);

    if (!bce_->emitJumpTarget(&finallyStart_))
        return false;

    if (controlInfo_) {
        // Fix up the gosubs that might have been emitted before non-local
        // jumps to the finally code.
        bce_->patchJumpsToTarget(controlInfo_->gosubs, finallyStart_);

        // Indicate that we're emitting a subroutine body.
        controlInfo_->setEmittingSubroutine();
    }
    if (finallyPos) {
        if (!bce_->updateSourceCoordNotes(finallyPos.value()))
            return false;
    }
    if (!bce_->emit1(JSOP_FINALLY))
        return false;

    if (retValKind_ == UseRetVal) {
        if (!bce_->emit1(JSOP_GETRVAL))
            return false;

        // Clear the frame's return value to make break/continue return
        // correct value even if there's no other statement before them:
        //
        //   eval("x: try { 1 } finally { break x; }"); // undefined, not 1
        if (!bce_->emit1(JSOP_UNDEFINED))
            return false;
        if (!bce_->emit1(JSOP_SETRVAL))
            return false;
    }

    state_ = Finally;
    return true;
}

bool
TryEmitter::emitFinallyEnd()
{
    MOZ_ASSERT(state_ == Finally);

    if (retValKind_ == UseRetVal) {
        if (!bce_->emit1(JSOP_SETRVAL))
            return false;
    }

    if (!bce_->emit1(JSOP_RETSUB))
        return false;

    bce_->hasTryFinally = true;
    return true;
}

bool
TryEmitter::emitEnd()
{
    if (state_ == Catch) {
        MOZ_ASSERT(!hasFinally());
        if (!emitCatchEnd(false))
            return false;
    } else {
        MOZ_ASSERT(state_ == Finally);
        MOZ_ASSERT(hasFinally());
        if (!emitFinallyEnd())
            return false;
    }

    MOZ_ASSERT(bce_->stackDepth == depth_);

    // ReconstructPCStack needs a NOP here to mark the end of the last
    // catch block.
    if (!bce_->emit1(JSOP_NOP))
        return false;

    // Fix up the end-of-try/catch jumps to come here.
    if (!bce_->emitJumpTargetAndPatch(catchAndFinallyJump_))
        return false;

    // Add the try note last, to let post-order give us the right ordering
    // (first to last for a given nesting level, inner to outer by level).
    if (hasCatch()) {
        if (!bce_->tryNoteList.append(JSTRY_CATCH, depth_, tryStart_, tryEnd_.offset))
            return false;
    }

    // If we've got a finally, mark try+catch region with additional
    // trynote to catch exceptions (re)thrown from a catch block or
    // for the try{}finally{} case.
    if (hasFinally()) {
        if (!bce_->tryNoteList.append(JSTRY_FINALLY, depth_, tryStart_, finallyStart_.offset))
            return false;
    }

    state_ = End;
    return true;
}
