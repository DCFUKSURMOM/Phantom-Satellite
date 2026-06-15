/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_loongarch64_CodeGenerator_loongarch64_h
#define jit_loongarch64_CodeGenerator_loongarch64_h

#include "jit/mips-shared/CodeGenerator-mips-shared.h"

namespace js {
namespace jit {

class CodeGeneratorLoongArch64 : public CodeGeneratorMIPSShared
{
  protected:
    void testNullEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                            MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        MOZ_ASSERT(value.valueReg() != SecondScratchReg);
        masm.splitTag(value.valueReg(), SecondScratchReg);
        emitBranch(SecondScratchReg, ImmTag(JSVAL_TAG_NULL), cond, ifTrue, ifFalse);
    }
    void testUndefinedEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                                 MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        MOZ_ASSERT(value.valueReg() != SecondScratchReg);
        masm.splitTag(value.valueReg(), SecondScratchReg);
        emitBranch(SecondScratchReg, ImmTag(JSVAL_TAG_UNDEFINED), cond, ifTrue, ifFalse);
    }
    void testObjectEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                              MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        MOZ_ASSERT(value.valueReg() != SecondScratchReg);
        masm.splitTag(value.valueReg(), SecondScratchReg);
        emitBranch(SecondScratchReg, ImmTag(JSVAL_TAG_OBJECT), cond, ifTrue, ifFalse);
    }

    void emitTableSwitchDispatch(MTableSwitch* mir, Register index, Register base);

    template <typename T>
    void emitWasmLoadI64(T* ins);
    template <typename T>
    void emitWasmStoreI64(T* ins);

  public:
    CodeGeneratorLoongArch64(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm)
      : CodeGeneratorMIPSShared(gen, graph, masm)
    { }

    void visitCompareB(LCompareB* lir);
    void visitCompareBAndBranch(LCompareBAndBranch* lir);
    void visitCompareBitwise(LCompareBitwise* lir);
    void visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir);
    void visitCompare(LCompare* comp);
    void visitCompareAndBranch(LCompareAndBranch* comp);
    void visitCompareI64(LCompareI64* lir);
    void visitCompareI64AndBranch(LCompareI64AndBranch* lir);
    void visitDivI(LDivI* ins);
    void visitModI(LModI* ins);
    void visitUDivOrMod(LUDivOrMod* ins);
    void visitDivOrModI64(LDivOrModI64* lir);
    void visitUDivOrModI64(LUDivOrModI64* lir);
    void visitWasmLoadI64(LWasmLoadI64* lir);
    void visitWasmUnalignedLoadI64(LWasmUnalignedLoadI64* lir);
    void visitWasmStoreI64(LWasmStoreI64* ins);
    void visitWasmUnalignedStoreI64(LWasmUnalignedStoreI64* ins);
    void visitWasmLoadGlobalVarI64(LWasmLoadGlobalVarI64* ins);
    void visitWasmStoreGlobalVarI64(LWasmStoreGlobalVarI64* ins);
    void visitWasmSelectI64(LWasmSelectI64* ins);
    void visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir);
    void visitWasmReinterpretToI64(LWasmReinterpretToI64* lir);
    void visitExtendInt32ToInt64(LExtendInt32ToInt64* lir);
    void visitWrapInt64ToInt32(LWrapInt64ToInt32* lir);
    void visitClzI64(LClzI64* lir);
    void visitCtzI64(LCtzI64* lir);
    void visitNotI64(LNotI64* lir);
    void visitWasmTruncateToInt32(LWasmTruncateToInt32* lir);
    void visitWasmTruncateToInt64(LWasmTruncateToInt64* lir);
    void visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir);
    void visitTestI64AndBranch(LTestI64AndBranch* lir);
    void visitWasmAddOffset(LWasmAddOffset* lir);
    void visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins);
    void visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins);
    void visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins);
    void visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins);
    void visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop* lir);
    void visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect* lir);
    void visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir);
    void visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir);

    void visitOutOfLineBailout(OutOfLineBailout* ool);
    void visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool);

  protected:
    ValueOperand ToValue(LInstruction* ins, size_t pos);
    ValueOperand ToOutValue(LInstruction* ins);
    ValueOperand ToTempValue(LInstruction* ins, size_t pos);

    Register splitTagForTest(const ValueOperand& value);

  public:
    void visitBox(LBox* box);
    void visitUnbox(LUnbox* unbox);

    void setReturnDoubleRegs(LiveRegisterSet* regs);
};

typedef CodeGeneratorLoongArch64 CodeGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_loongarch64_CodeGenerator_loongarch64_h */
