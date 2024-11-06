/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CallOrNewEmitter_h
#define frontend_CallOrNewEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "frontend/ElemOpEmitter.h"
#include "frontend/IfEmitter.h"
#include "frontend/PropOpEmitter.h"
#include "frontend/ValueUsage.h"
#include "js/TypeDecls.h"
#include "vm/Opcodes.h"
#include "jsopcode.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

class MOZ_RAII AutoEmittingRunOnceLambda
{
    BytecodeEmitter* bce_;

  public:
    explicit AutoEmittingRunOnceLambda(BytecodeEmitter* bce);
    ~AutoEmittingRunOnceLambda();
};

// Class for emitting bytecode for call or new expression.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `print(arg);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.emitNameCallee();
//     emit(print);
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `callee.prop(arg1, arg2);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     PropOpEmitter& poe = cone.prepareForPropCallee(false);
//     ... emit `callee.prop` with `poe` here...
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg1);
//     emit(arg2);
//     cone.emitEnd(2, Some(offset_of_callee));
//
//   `callee[key](arg);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     ElemOpEmitter& eoe = cone.prepareForElemCallee(false);
//     ... emit `callee[key]` with `eoe` here...
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `(function() { ... })(arg);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.prepareForFunctionCallee();
//     emit(function);
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `super(arg);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.emitSuperCallee();
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `(some_other_expression)(arg);`
//     CallOrNewEmitter cone(this, JSOP_CALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.prepareForOtherCallee();
//     emit(some_other_expression);
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `print(...arg);`
//     CallOrNewEmitter cone(this, JSOP_SPREADCALL,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.emitNameCallee();
//     emit(print);
//     cone.emitThis();
//     if (cone.wantSpreadOperand())
//       emit(arg)
//     cone.emitSpreadArgumentsTest();
//     emit([...arg]);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `print(...rest);`
//   where `rest` is rest parameter
//     CallOrNewEmitter cone(this, JSOP_SPREADCALL,
//                           CallOrNewEmitter::ArgumentsKind::SingleSpreadRest,
//                           ValueUsage::WantValue);
//     cone.emitNameCallee();
//     emit(print);
//     cone.emitThis();
//     if (cone.wantSpreadOperand())
//       emit(arg)
//     cone.emitSpreadArgumentsTest();
//     emit([...arg]);
//     cone.emitEnd(1, Some(offset_of_callee));
//
//   `new f(arg);`
//     CallOrNewEmitter cone(this, JSOP_NEW,
//                           CallOrNewEmitter::ArgumentsKind::Other,
//                           ValueUsage::WantValue);
//     cone.emitNameCallee();
//     emit(f);
//     cone.emitThis();
//     cone.prepareForNonSpreadArguments();
//     emit(arg);
//     cone.emitEnd(1, Some(offset_of_callee));
//
class MOZ_STACK_CLASS CallOrNewEmitter
{
  public:
    enum class ArgumentsKind {
        Other,

        // Specify this for the following case:
        //
        //   function f(...rest) {
        //     g(...rest);
        //   }
        //
        // This enables optimization to avoid allocating an intermediate array
        // for spread operation.
        //
        // wantSpreadOperand() returns true when this is specified.
        SingleSpreadRest
    };

  private:
    BytecodeEmitter* bce_;

    // The opcode for the call or new.
    JSOp op_;

    // Whether the call is a spread call with single rest parameter or not.
    // See the comment in emitSpreadArgumentsTest for more details.
    ArgumentsKind argumentsKind_;

    // The branch for spread call optimization.
    mozilla::Maybe<InternalIfEmitter> ifNotOptimizable_;

    mozilla::Maybe<AutoEmittingRunOnceLambda> autoEmittingRunOnceLambda_;

    mozilla::Maybe<PropOpEmitter> poe_;
    mozilla::Maybe<ElemOpEmitter> eoe_;

    // The state of this emitter.
    //
    // +-------+   emitNameCallee           +------------+
    // | Start |-+------------------------->| NameCallee |------+
    // +-------+ |                          +------------+      |
    //           |                                              |
    //           | prepareForPropCallee     +------------+      v
    //           +------------------------->| PropCallee |----->+
    //           |                          +------------+      |
    //           |                                              |
    //           | prepareForElemCallee     +------------+      v
    //           +------------------------->| ElemCallee |----->+
    //           |                          +------------+      |
    //           |                                              |
    //           | prepareForFunctionCallee +----------------+  v
    //           +------------------------->| FunctionCallee |->+
    //           |                          +----------------+  |
    //           |                                              |
    //           | emitSuperCallee          +-------------+     v
    //           +------------------------->| SuperCallee |---->+
    //           |                          +-------------+     |
    //           |                                              |
    //           | prepareForOtherCallee    +-------------+     v
    //           +------------------------->| OtherCallee |---->+
    //                                      +-------------+     |
    //                                                          |
    // +--------------------------------------------------------+
    // |
    // | emitThis +------+
    // +--------->| This |-+
    //            +------+ |
    //                     |
    // +-------------------+
    // |
    // | [!isSpread]
    // |   prepareForNonSpreadArguments    +-----------+ emitEnd +-----+
    // +------------------------------->+->| Arguments |-------->| End |
    // |                                ^  +-----------+         +-----+
    // |                                |
    // |                                +----------------------------------+
    // |                                                                   |
    // | [isSpread]                                                        |
    // |   wantSpreadOperand +-------------------+ emitSpreadArgumentsTest |
    // +-------------------->| WantSpreadOperand |-------------------------+
    //                       +-------------------+
    enum class State {
        // The initial state.
        Start,

        // After calling emitNameCallee.
        NameCallee,

        // After calling prepareForPropCallee.
        PropCallee,

        // After calling prepareForElemCallee.
        ElemCallee,

        // After calling prepareForFunctionCallee.
        FunctionCallee,

        // After calling emitSuperCallee.
        SuperCallee,

        // After calling prepareForOtherCallee.
        OtherCallee,

        // After calling emitThis.
        This,

        // After calling wantSpreadOperand.
        WantSpreadOperand,

        // After calling prepareForNonSpreadArguments.
        Arguments,

        // After calling emitEnd.
        End
    };
    State state_ = State::Start;

  public:
    CallOrNewEmitter(BytecodeEmitter* bce, JSOp op,
                     ArgumentsKind argumentsKind,
                     ValueUsage valueUsage);

  private:
    [[nodiscard]] bool isCall() const {
        return op_ == JSOP_CALL || op_ == JSOP_CALL_IGNORES_RV ||
               op_ == JSOP_SPREADCALL ||
               isEval() || isFunApply() || isFunCall();
    }

    [[nodiscard]] bool isNew() const {
        return op_ == JSOP_NEW || op_ == JSOP_SPREADNEW;
    }

    [[nodiscard]] bool isSuperCall() const {
        return op_ == JSOP_SUPERCALL || op_ == JSOP_SPREADSUPERCALL;
    }

    [[nodiscard]] bool isEval() const {
        return op_ == JSOP_EVAL || op_ == JSOP_STRICTEVAL ||
               op_ == JSOP_SPREADEVAL || op_ == JSOP_STRICTSPREADEVAL;
    }

    [[nodiscard]] bool isFunApply() const {
        return op_ == JSOP_FUNAPPLY;
    }

    [[nodiscard]] bool isFunCall() const {
        return op_ == JSOP_FUNCALL;
    }

    [[nodiscard]] bool isSpread() const {
        return JOF_OPTYPE(op_) == JOF_BYTE;
    }

    [[nodiscard]] bool isSingleSpreadRest() const {
        return argumentsKind_ == ArgumentsKind::SingleSpreadRest;
    }

  public:
    [[nodiscard]] bool emitNameCallee(JSAtom* name);
    [[nodiscard]] PropOpEmitter& prepareForPropCallee(bool isSuperProp);
    [[nodiscard]] ElemOpEmitter& prepareForElemCallee(bool isSuperElem);
    [[nodiscard]] bool prepareForFunctionCallee();
    [[nodiscard]] bool emitSuperCallee();
    [[nodiscard]] bool prepareForOtherCallee();

    [[nodiscard]] bool emitThis();

    // Used by BytecodeEmitter::emitPipeline to reuse CallOrNewEmitter instance
    // across multiple chained calls.
    void reset();

    [[nodiscard]] bool prepareForNonSpreadArguments();

    // See the usage in the comment at the top of the class.
    [[nodiscard]] bool wantSpreadOperand();
    [[nodiscard]] bool emitSpreadArgumentsTest();

    // Parameters are the offset in the source code for each character below:
    //
    //   callee(arg);
    //   ^
    //   |
    //   beginPos
    //
    // Can be Nothing() if not available.
    [[nodiscard]] bool emitEnd(uint32_t argc, const mozilla::Maybe<uint32_t>& beginPos);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_CallOrNewEmitter_h */
