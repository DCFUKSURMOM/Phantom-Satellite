/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_EmitterScope_h
#define frontend_EmitterScope_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "ds/Nestable.h"
#include "frontend/NameAnalysisTypes.h"
#include "frontend/NameCollections.h"
#include "frontend/SharedContext.h"
#include "jstypes.h"
#include "vm/Scope.h"

namespace js {
namespace frontend {

// A scope that introduces bindings.
class EmitterScope : public Nestable<EmitterScope>
{
    // The cache of bound names that may be looked up in the
    // scope. Initially populated as the set of names this scope binds. As
    // names are looked up in enclosing scopes, they are cached on the
    // current scope.
    PooledMapPtr<NameLocationMap> nameCache_;

    // If this scope's cache does not include free names, such as the
    // global scope, the NameLocation to return.
    mozilla::Maybe<NameLocation> fallbackFreeNameLocation_;

    // True if there is a corresponding EnvironmentObject on the environment
    // chain, false if all bindings are stored in frame slots on the stack.
    bool hasEnvironment_;

    // The number of enclosing environments. Used for error checking.
    uint8_t environmentChainLength_;

    // The next usable slot on the frame for not-closed over bindings.
    //
    // The initial frame slot when assigning slots to bindings is the
    // enclosing scope's nextFrameSlot. For the first scope in a frame,
    // the initial frame slot is 0.
    uint32_t nextFrameSlot_;

    // The index in the BytecodeEmitter's interned scope vector, otherwise
    // ScopeNote::NoScopeIndex.
    uint32_t scopeIndex_;

    // If kind is Lexical, Catch, or With, the index in the BytecodeEmitter's
    // block scope note list. Otherwise ScopeNote::NoScopeNote.
    uint32_t noteIndex_;

    [[nodiscard]] bool ensureCache(BytecodeEmitter* bce);

    template <typename BindingIter>
    [[nodiscard]] bool checkSlotLimits(BytecodeEmitter* bce, const BindingIter& bi);

    [[nodiscard]] bool checkEnvironmentChainLength(BytecodeEmitter* bce);

    void updateFrameFixedSlots(BytecodeEmitter* bce, const BindingIter& bi);

    [[nodiscard]] bool putNameInCache(BytecodeEmitter* bce, JSAtom* name, NameLocation loc);

    mozilla::Maybe<NameLocation> lookupInCache(BytecodeEmitter* bce, JSAtom* name);

    EmitterScope* enclosing(BytecodeEmitter** bce) const;

    Scope* enclosingScope(BytecodeEmitter* bce) const;

    static bool nameCanBeFree(BytecodeEmitter* bce, JSAtom* name);

    static NameLocation searchInEnclosingScope(JSAtom* name, Scope* scope, uint8_t hops);
    NameLocation searchAndCache(BytecodeEmitter* bce, JSAtom* name);

    template <typename ScopeCreator>
    [[nodiscard]] bool internScope(BytecodeEmitter* bce, ScopeCreator createScope);
    template <typename ScopeCreator>
    [[nodiscard]] bool internBodyScope(BytecodeEmitter* bce, ScopeCreator createScope);
    [[nodiscard]] bool appendScopeNote(BytecodeEmitter* bce);

    [[nodiscard]] bool deadZoneFrameSlotRange(BytecodeEmitter* bce, uint32_t slotStart,
                                             uint32_t slotEnd) const;

  public:
    explicit EmitterScope(BytecodeEmitter* bce);

    void dump(BytecodeEmitter* bce);

    [[nodiscard]] bool enterLexical(BytecodeEmitter* bce, ScopeKind kind,
                                   Handle<LexicalScope::Data*> bindings);
    [[nodiscard]] bool enterNamedLambda(BytecodeEmitter* bce, FunctionBox* funbox);
    [[nodiscard]] bool enterComprehensionFor(BytecodeEmitter* bce,
                                            Handle<LexicalScope::Data*> bindings);
    [[nodiscard]] bool enterFunction(BytecodeEmitter* bce, FunctionBox* funbox);
    [[nodiscard]] bool enterFunctionExtraBodyVar(BytecodeEmitter* bce, FunctionBox* funbox);
    [[nodiscard]] bool enterParameterExpressionVar(BytecodeEmitter* bce);
    [[nodiscard]] bool enterGlobal(BytecodeEmitter* bce, GlobalSharedContext* globalsc);
    [[nodiscard]] bool enterEval(BytecodeEmitter* bce, EvalSharedContext* evalsc);
    [[nodiscard]] bool enterModule(BytecodeEmitter* module, ModuleSharedContext* modulesc);
    [[nodiscard]] bool enterWith(BytecodeEmitter* bce);
    [[nodiscard]] bool deadZoneFrameSlots(BytecodeEmitter* bce) const;

    [[nodiscard]] bool leave(BytecodeEmitter* bce, bool nonLocal = false);

    uint32_t index() const {
        MOZ_ASSERT(scopeIndex_ != ScopeNote::NoScopeIndex, "Did you forget to intern a Scope?");
        return scopeIndex_;
    }

    uint32_t noteIndex() const {
        return noteIndex_;
    }

    Scope* scope(const BytecodeEmitter* bce) const;

    bool hasEnvironment() const {
        return hasEnvironment_;
    }

    // The first frame slot used.
    uint32_t frameSlotStart() const {
        if (EmitterScope* inFrame = enclosingInFrame())
            return inFrame->nextFrameSlot_;
        return 0;
    }

    // The last frame slot used + 1.
    uint32_t frameSlotEnd() const {
        return nextFrameSlot_;
    }

    uint32_t numFrameSlots() const {
        return frameSlotEnd() - frameSlotStart();
    }

    EmitterScope* enclosingInFrame() const {
        return Nestable<EmitterScope>::enclosing();
    }

    NameLocation lookup(BytecodeEmitter* bce, JSAtom* name);

    mozilla::Maybe<NameLocation> locationBoundInScope(JSAtom* name, EmitterScope* target);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_EmitterScope_h */
