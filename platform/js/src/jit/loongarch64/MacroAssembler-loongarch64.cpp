/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/loongarch64/MacroAssembler-loongarch64.h"

#include "jsmath.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/JitFrames.h"
#include "jit/loongarch64/SharedICRegisters-loongarch64.h"
#include "jit/MacroAssembler.h"
#include "jit/MoveEmitter.h"
#include "vm/Stack.h"
#include "jscntxt.h"

#include "jit/MacroAssembler-inl.h"

namespace js {
namespace jit {

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_div(Register lhs, Register rhs)
{
  pendingDivKind_ = PendingDivW;
  pendingDivLhs_ = lhs;
  pendingDivRhs_ = rhs;
  return m_buffer.nextOffset();
}

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_divu(Register lhs, Register rhs)
{
  pendingDivKind_ = PendingDivWU;
  pendingDivLhs_ = lhs;
  pendingDivRhs_ = rhs;
  return m_buffer.nextOffset();
}

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_ddiv(Register lhs, Register rhs)
{
  pendingDivKind_ = PendingDivD;
  pendingDivLhs_ = lhs;
  pendingDivRhs_ = rhs;
  return m_buffer.nextOffset();
}

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_ddivu(Register lhs, Register rhs)
{
  pendingDivKind_ = PendingDivDU;
  pendingDivLhs_ = lhs;
  pendingDivRhs_ = rhs;
  return m_buffer.nextOffset();
}

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_mflo(Register dest)
{
  switch (pendingDivKind_) {
    case PendingDivW:
      return as_div_w(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivWU:
      return as_div_wu(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivD:
      return as_div_d(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivDU:
      return as_div_du(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivNone:
      break;
  }
  MOZ_CRASH("mflo without pending div");
}

BufferOffset
MacroAssemblerLOONGARCH64Compat::as_mfhi(Register dest)
{
  switch (pendingDivKind_) {
    case PendingDivW:
      return as_mod_w(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivWU:
      return as_mod_wu(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivD:
      return as_mod_d(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivDU:
      return as_mod_du(dest, pendingDivLhs_, pendingDivRhs_);
    case PendingDivNone:
      break;
  }
  MOZ_CRASH("mfhi without pending div");
}

namespace {

static inline void MoveAtomicValue(MacroAssemblerLOONGARCH64Compat& masm,
                                   Imm32 value, Register dest) {
  masm.move32(value, dest);
}

static inline void MoveAtomicValue(MacroAssemblerLOONGARCH64Compat& masm,
                                   Register value, Register dest) {
  masm.move32(value, dest);
}

static inline void SignExtendAtomicResult(MacroAssemblerLOONGARCH64Compat& masm,
                                          int nbytes, Register output) {
  switch (nbytes) {
    case 1:
      masm.as_ext_w_b(output, output);
      break;
    case 2:
      masm.as_ext_w_h(output, output);
      break;
    case 4:
      break;
    default:
      MOZ_CRASH("unexpected atomic width");
  }
}

static inline void ZeroExtendAtomicResult(MacroAssemblerLOONGARCH64Compat& masm,
                                          int nbytes, Register output) {
  switch (nbytes) {
    case 1:
      masm.as_bstrpick_d(output, output, 7, 0);
      break;
    case 2:
      masm.as_bstrpick_d(output, output, 15, 0);
      break;
    case 4:
      break;
    default:
      MOZ_CRASH("unexpected atomic width");
  }
}

template <typename T>
static inline void PrepareAtomicAddress(
    MacroAssemblerLOONGARCH64Compat& masm, int nbytes, const T& address,
    Register offsetTemp, Register maskTemp) {
  masm.computeEffectiveAddress(address, ScratchRegister);
  masm.as_andi(offsetTemp, ScratchRegister, 3);
  masm.as_sub_d(ScratchRegister, ScratchRegister, offsetTemp);
  masm.as_slli_w(offsetTemp, offsetTemp, 3);
  masm.ma_li(maskTemp, Imm32(int32_t(UINT32_MAX >> ((4 - nbytes) * 8))));
  masm.as_sll_w(maskTemp, maskTemp, offsetTemp);
}

template <typename S, typename T>
static void AtomicFetchOpLoongArch64(MacroAssemblerLOONGARCH64Compat& masm,
                                     int nbytes, bool signExtend, AtomicOp op,
                                     const S& value, const T& address,
                                     Register valueTemp, Register offsetTemp,
                                     Register maskTemp, Register output) {
  Label again;

  PrepareAtomicAddress(masm, nbytes, address, offsetTemp, maskTemp);

  masm.bind(&again);
  masm.as_dbar(0);
  masm.as_ll_w(SecondScratchReg, ScratchRegister, 0);

  if (output != InvalidReg) {
    masm.as_and(output, SecondScratchReg, maskTemp);
    masm.as_srl_w(output, output, offsetTemp);
    if (signExtend) {
      SignExtendAtomicResult(masm, nbytes, output);
    }
  }

  MoveAtomicValue(masm, value, valueTemp);
  masm.as_sll_w(valueTemp, valueTemp, offsetTemp);

  switch (op) {
    case AtomicFetchAddOp:
      masm.as_add_w(valueTemp, SecondScratchReg, valueTemp);
      break;
    case AtomicFetchSubOp:
      masm.as_sub_w(valueTemp, SecondScratchReg, valueTemp);
      break;
    case AtomicFetchAndOp:
      masm.as_and(valueTemp, SecondScratchReg, valueTemp);
      break;
    case AtomicFetchOrOp:
      masm.as_or(valueTemp, SecondScratchReg, valueTemp);
      break;
    case AtomicFetchXorOp:
      masm.as_xor(valueTemp, SecondScratchReg, valueTemp);
      break;
    default:
      MOZ_CRASH("unexpected atomic op");
  }

  masm.as_and(valueTemp, valueTemp, maskTemp);
  masm.as_or(SecondScratchReg, SecondScratchReg, maskTemp);
  masm.as_xor(SecondScratchReg, SecondScratchReg, maskTemp);
  masm.as_or(SecondScratchReg, SecondScratchReg, valueTemp);
  masm.as_sc_w(SecondScratchReg, ScratchRegister, 0);
  masm.ma_b(SecondScratchReg, SecondScratchReg, &again, Assembler::Zero,
            ShortJump);
  masm.as_dbar(0);
}

template <typename T>
static void CompareExchangeLoongArch64(
    MacroAssemblerLOONGARCH64Compat& masm, int nbytes, bool signExtend,
    const T& address, Register oldval, Register newval, Register valueTemp,
    Register offsetTemp, Register maskTemp, Register output) {
  Label again, done;

  PrepareAtomicAddress(masm, nbytes, address, offsetTemp, maskTemp);

  masm.bind(&again);
  masm.as_dbar(0);
  masm.as_ll_w(SecondScratchReg, ScratchRegister, 0);

  masm.as_and(output, SecondScratchReg, maskTemp);
  masm.as_srl_w(output, output, offsetTemp);
  if (signExtend) {
    SignExtendAtomicResult(masm, nbytes, output);
  }

  if (oldval != InvalidReg) {
    masm.move32(oldval, valueTemp);
    if (nbytes != 4) {
      if (signExtend) {
        SignExtendAtomicResult(masm, nbytes, valueTemp);
      } else {
        ZeroExtendAtomicResult(masm, nbytes, valueTemp);
      }
    }
    masm.ma_b(output, valueTemp, &done, Assembler::NotEqual, ShortJump);
  }

  masm.move32(newval, valueTemp);
  masm.as_sll_w(valueTemp, valueTemp, offsetTemp);
  masm.as_and(valueTemp, valueTemp, maskTemp);
  masm.as_or(SecondScratchReg, SecondScratchReg, maskTemp);
  masm.as_xor(SecondScratchReg, SecondScratchReg, maskTemp);
  masm.as_or(SecondScratchReg, SecondScratchReg, valueTemp);
  masm.as_sc_w(SecondScratchReg, ScratchRegister, 0);
  masm.ma_b(SecondScratchReg, SecondScratchReg, &again, Assembler::Zero,
            ShortJump);
  masm.as_dbar(0);

  masm.bind(&done);
}

}  // namespace

void MacroAssembler::alignFrameForICArguments(AfterICSaveLive& aic) {
  if (framePushed() % ABIStackAlignment != 0) {
    aic.alignmentPadding = ABIStackAlignment - (framePushed() % ABIStackAlignment);
    reserveStack(aic.alignmentPadding);
  } else {
    aic.alignmentPadding = 0;
  }
  MOZ_ASSERT(framePushed() % ABIStackAlignment == 0);
}

void MacroAssembler::restoreFrameAlignmentForICArguments(AfterICSaveLive& aic) {
  if (aic.alignmentPadding != 0) {
    freeStack(aic.alignmentPadding);
  }
}

void MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output) {
  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  as_ftintrne_l_d(fpscratch, input);
  as_movfr2gr_d(output, fpscratch);
  // if (res < 0); res = 0;
  as_slt(scratch, output, zero);
  as_masknez(output, output, scratch);
  // if res > 255; res = 255;
  as_sltui(scratch, output, 255);
  as_addi_d(output, output, -255);
  as_maskeqz(output, output, scratch);
  as_addi_d(output, output, 255);
}

bool MacroAssemblerLOONGARCH64Compat::buildOOLFakeExitFrame(void* fakeReturnAddr) {
  uint32_t descriptor = MakeFrameDescriptor(
      asMasm().framePushed(), JitFrame_IonJS, ExitFrameLayout::Size());

  asMasm().Push(Imm32(descriptor));  // descriptor_
  asMasm().Push(ImmPtr(fakeReturnAddr));

  return true;
}

void MacroAssemblerLOONGARCH64Compat::convertUInt32ToDouble(Register src,
                                                        FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  as_bstrpick_d(scratch, src, 31, 0);
  as_movgr2fr_d(dest, scratch);
  as_ffint_d_l(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertInt64ToDouble(Register src,
                                                           FloatRegister dest) {
  as_movgr2fr_d(dest, src);
  as_ffint_d_l(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertInt64ToFloat32(Register src,
                                                            FloatRegister dest) {
  as_movgr2fr_d(dest, src);
  as_ffint_s_l(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertUInt64ToDouble(Register src,
                                                        FloatRegister dest) {
  Label positive, done;
  ma_b(src, src, &positive, NotSigned, ShortJump);
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());

  MOZ_ASSERT(src != scratch);
  MOZ_ASSERT(src != scratch2);

  ma_and(scratch, src, Imm32(1));
  as_srli_d(scratch2, src, 1);
  as_or(scratch, scratch, scratch2);
  as_movgr2fr_d(dest, scratch);
  as_ffint_d_l(dest, dest);
  asMasm().addDouble(dest, dest);
  ma_b(&done, ShortJump);

  bind(&positive);
  as_movgr2fr_d(dest, src);
  as_ffint_d_l(dest, dest);

  bind(&done);
}

bool MacroAssemblerLOONGARCH64Compat::convertUInt64ToDoubleNeedsTemp() {
  return false;
}

void MacroAssemblerLOONGARCH64Compat::convertUInt64ToDouble(
    Register64 src, FloatRegister dest, Register temp) {
  MOZ_ASSERT(temp == InvalidReg);
  convertUInt64ToDouble(src.reg, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertUInt32ToFloat32(Register src,
                                                         FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  as_bstrpick_d(scratch, src, 31, 0);
  as_movgr2fr_d(dest, scratch);
  as_ffint_s_l(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertUInt64ToFloat32(Register src,
                                                             FloatRegister dest) {
  Label positive, done;
  ma_b(src, src, &positive, NotSigned, ShortJump);
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());

  MOZ_ASSERT(src != scratch);
  MOZ_ASSERT(src != scratch2);

  ma_and(scratch, src, Imm32(1));
  as_srli_d(scratch2, src, 1);
  as_or(scratch, scratch, scratch2);
  as_movgr2fr_d(dest, scratch);
  as_ffint_s_l(dest, dest);
  asMasm().addFloat32(dest, dest);
  ma_b(&done, ShortJump);

  bind(&positive);
  as_movgr2fr_d(dest, src);
  as_ffint_s_l(dest, dest);

  bind(&done);
}

void MacroAssemblerLOONGARCH64Compat::convertDoubleToFloat32(FloatRegister src,
                                                         FloatRegister dest) {
  as_fcvt_s_d(dest, src);
}

const int CauseBitPos = int(Assembler::CauseI);
const int CauseBitCount = 1 + int(Assembler::CauseV) - int(Assembler::CauseI);
const int CauseIOrVMask = ((1 << int(Assembler::CauseI)) |
                           (1 << int(Assembler::CauseV))) >>
                          int(Assembler::CauseI);

// Checks whether a double is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void MacroAssemblerLOONGARCH64Compat::convertDoubleToInt32(FloatRegister src,
                                                       Register dest,
                                                       Label* fail,
                                                       bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromDouble(src, dest);
    as_rotri_d(dest, dest, 63);
    ma_b(dest, Imm32(1), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchFloat32Scope fpscratch(asMasm());
  // Truncate double to int ; if result is inexact or invalid fail.
  as_ftintrz_w_d(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromFloat32(fpscratch, dest);
  as_bstrpick_d(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch,
          CauseIOrVMask);  // masking for Inexact and Invalid flag.
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64Compat::convertDoubleToPtr(FloatRegister src,
                                                     Register dest, Label* fail,
                                                     bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromDouble(src, dest);
    as_rotri_d(dest, dest, 63);
    ma_b(dest, Imm32(1), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  // Truncate double to int64 ; if result is inexact or invalid fail.
  as_ftintrz_l_d(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromDouble(fpscratch, dest);
  as_bstrpick_d(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch,
          CauseIOrVMask);  // masking for Inexact and Invalid flag.
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

// Checks whether a float32 is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void MacroAssemblerLOONGARCH64Compat::convertFloat32ToInt32(
    FloatRegister src, Register dest, Label* fail, bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromFloat32(src, dest);
    ma_b(dest, Imm32(INT32_MIN), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchFloat32Scope fpscratch(asMasm());
  as_ftintrz_w_s(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromFloat32(fpscratch, dest);
  as_bstrpick_d(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch, CauseIOrVMask);
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64Compat::convertFloat32ToDouble(FloatRegister src,
                                                         FloatRegister dest) {
  as_fcvt_d_s(dest, src);
}

void MacroAssemblerLOONGARCH64Compat::convertInt32ToFloat32(Register src,
                                                        FloatRegister dest) {
  as_movgr2fr_w(dest, src);
  as_ffint_s_w(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::convertInt32ToFloat32(const Address& src,
                                                        FloatRegister dest) {
  ma_fld_s(dest, src);
  as_ffint_s_w(dest, dest);
}

void MacroAssemblerLOONGARCH64Compat::movq(Register rj, Register rd) {
  as_or(rd, rj, zero);
}

void MacroAssemblerLOONGARCH64::ma_li(Register dest, CodeOffset* label) {
  BufferOffset bo = m_buffer.nextOffset();
  ma_liPatchable(dest, ImmWord(/* placeholder */ 0));
  label->bind(bo.getOffset());
}

void MacroAssemblerLOONGARCH64::ma_li(Register dest, ImmWord imm) {
  int64_t value = imm.value;

  if (-1 == (value >> 11) || 0 == (value >> 11)) {
    as_addi_w(dest, zero, value);
    return;
  }

  if (0 == (value >> 12)) {
    as_ori(dest, zero, value);
    return;
  }

  if (-1 == (value >> 31) || 0 == (value >> 31)) {
    as_lu12i_w(dest, (value >> 12) & 0xfffff);
  } else if (0 == (value >> 32)) {
    as_lu12i_w(dest, (value >> 12) & 0xfffff);
    as_bstrins_d(dest, zero, 63, 32);
  } else if (-1 == (value >> 51) || 0 == (value >> 51)) {
    if (is_uintN((value >> 12) & 0xfffff, 20)) {
      as_lu12i_w(dest, (value >> 12) & 0xfffff);
    }
    as_lu32i_d(dest, (value >> 32) & 0xfffff);
  } else if (0 == (value >> 52)) {
    if (is_uintN((value >> 12) & 0xfffff, 20)) {
      as_lu12i_w(dest, (value >> 12) & 0xfffff);
    }
    as_lu32i_d(dest, (value >> 32) & 0xfffff);
    as_bstrins_d(dest, zero, 63, 52);
  } else {
    if (is_uintN((value >> 12) & 0xfffff, 20)) {
      as_lu12i_w(dest, (value >> 12) & 0xfffff);
    }
    if (is_uintN((value >> 32) & 0xfffff, 20)) {
      as_lu32i_d(dest, (value >> 32) & 0xfffff);
    }
    as_lu52i_d(dest, dest, (value >> 52) & 0xfff);
  }

  if (is_uintN(value & 0xfff, 12)) {
    as_ori(dest, dest, value & 0xfff);
  }
}

// This method generates lu32i_d, lu12i_w and ori instruction block that can be
// modified by UpdateLoad64Value, either during compilation (eg.
// Assembler::bind), or during execution (eg. jit::PatchJump).
void MacroAssemblerLOONGARCH64::ma_liPatchable(Register dest, ImmPtr imm) {
  return ma_liPatchable(dest, ImmWord(uintptr_t(imm.value)));
}

void MacroAssemblerLOONGARCH64::ma_liPatchable(Register dest, ImmWord imm,
                                           LiFlags flags) {
  // hi12, hi20, low20, low12
  if (Li64 == flags) {  // Li64: Imm data
    m_buffer.ensureSpace(4 * sizeof(uint32_t));
    as_lu12i_w(dest, imm.value >> 12 & 0xfffff);      // low20
    as_ori(dest, dest, imm.value & 0xfff);            // low12
    as_lu32i_d(dest, imm.value >> 32 & 0xfffff);      // hi20
    as_lu52i_d(dest, dest, imm.value >> 52 & 0xfff);  // hi12
  } else {                                            // Li48 address
    m_buffer.ensureSpace(3 * sizeof(uint32_t));
    as_lu12i_w(dest, imm.value >> 12 & 0xfffff);  // low20
    as_ori(dest, dest, imm.value & 0xfff);        // low12
    as_lu32i_d(dest, imm.value >> 32 & 0xfffff);  // hi20
  }
}

// Memory access ops.

void MacroAssemblerLOONGARCH64::ma_ld_b(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_b(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_b(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_b(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_bu(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_bu(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_bu(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_bu(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_h(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_h(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_h(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_h(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_hu(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_hu(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_hu(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_hu(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_w(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_w(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_w(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_w(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_wu(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_wu(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_wu(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_wu(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_ld_d(Register dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_ld_d(dest, base, offset);
  } else if (base != dest) {
    ma_li(dest, Imm32(offset));
    as_ldx_d(dest, base, dest);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_ldx_d(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_st_b(Register src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_st_b(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(src != scratch);
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_stx_b(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_st_h(Register src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_st_h(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(src != scratch);
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_stx_h(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_st_w(Register src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_st_w(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(src != scratch);
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_stx_w(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_st_d(Register src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_st_d(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(src != scratch);
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_stx_d(src, base, scratch);
  }
}

// Arithmetic-based ops.

// Add.
void MacroAssemblerLOONGARCH64::ma_add_d(Register rd, Register rj, Imm32 imm) {
  if (is_intN(imm.value, 12)) {
    as_addi_d(rd, rj, imm.value);
  } else if (rd != rj) {
    ma_li(rd, imm);
    as_add_d(rd, rj, rd);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_add_d(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_add32TestOverflow(Register rd, Register rj,
                                                 Register rk, Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  as_add_d(scratch, rj, rk);
  as_add_w(rd, rj, rk);
  ma_b(rd, Register(scratch), overflow, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64::ma_add32TestOverflow(Register rd, Register rj,
                                                 Imm32 imm, Label* overflow) {
  // Check for signed range because of as_addi_d
  if (is_intN(imm.value, 12)) {
    ScratchRegisterScope scratch(asMasm());
    as_addi_d(scratch, rj, imm.value);
    as_addi_w(rd, rj, imm.value);
    ma_b(rd, scratch, overflow, Assembler::NotEqual);
  } else {
    SecondScratchRegisterScope scratch2(asMasm());
    ma_li(scratch2, imm);
    ma_add32TestOverflow(rd, rj, scratch2, overflow);
  }
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestOverflow(Register rd, Register rj,
                                                  Register rk,
                                                  Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(rd != scratch);

  if (rj == rk) {
    if (rj == rd) {
      as_or(scratch, rj, zero);
      rj = scratch;
    }

    as_add_d(rd, rj, rj);
    as_xor(scratch, rj, rd);
    ma_b(scratch, zero, overflow, Assembler::LessThan);
  } else {
    SecondScratchRegisterScope scratch2(asMasm());
    MOZ_ASSERT(rj != scratch);
    MOZ_ASSERT(rd != scratch2);

    if (rj == rd) {
      as_or(scratch2, rj, zero);
      rj = scratch2;
    }

    as_add_d(rd, rj, rk);
    as_slti(scratch, rj, 0);
    as_slt(scratch2, rd, rj);
    ma_b(scratch, Register(scratch2), overflow, Assembler::NotEqual);
  }
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestOverflow(Register rd, Register rj,
                                                  Imm32 imm, Label* overflow) {
  SecondScratchRegisterScope scratch2(asMasm());

  if (imm.value == 0) {
    as_ori(rd, rj, 0);
    return;
  }

  if (rj == rd) {
    as_ori(scratch2, rj, 0);
    rj = scratch2;
  }

  ma_add_d(rd, rj, imm);

  if (imm.value > 0) {
    ma_b(rd, rj, overflow, Assembler::LessThan);
  } else {
    MOZ_ASSERT(imm.value < 0);
    ma_b(rd, rj, overflow, Assembler::GreaterThan);
  }
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestOverflow(Register rd, Register rj,
                                                  ImmWord imm,
                                                  Label* overflow) {
  SecondScratchRegisterScope scratch2(asMasm());

  if (imm.value == 0) {
    as_ori(rd, rj, 0);
    return;
  }

  if (rj == rd) {
    MOZ_ASSERT(rj != scratch2);
    as_ori(scratch2, rj, 0);
    rj = scratch2;
  }

  ma_li(rd, imm);
  as_add_d(rd, rj, rd);

  if (imm.value > 0) {
    ma_b(rd, rj, overflow, Assembler::LessThan);
  } else {
    MOZ_ASSERT(imm.value < 0);
    ma_b(rd, rj, overflow, Assembler::GreaterThan);
  }
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestCarry(Condition cond, Register rd,
                                               Register rj, Register rk,
                                               Label* label) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(rd != rk);
  MOZ_ASSERT(rd != scratch);
  as_add_d(rd, rj, rk);
  as_sltu(scratch, rd, rk);
  ma_b(scratch, Register(scratch), label,
       cond == Assembler::CarrySet ? Assembler::NonZero : Assembler::Zero);
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestCarry(Condition cond, Register rd,
                                               Register rj, Imm32 imm,
                                               Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());

  // Check for signed range because of as_addi_d
  if (is_intN(imm.value, 12)) {
    as_addi_d(rd, rj, imm.value);
    as_sltui(scratch2, rd, imm.value);
    ma_b(scratch2, scratch2, label,
         cond == Assembler::CarrySet ? Assembler::NonZero : Assembler::Zero);
  } else {
    ma_li(scratch2, imm);
    ma_addPtrTestCarry(cond, rd, rj, scratch2, label);
  }
}

void MacroAssemblerLOONGARCH64::ma_addPtrTestCarry(Condition cond, Register rd,
                                               Register rj, ImmWord imm,
                                               Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());

  // Check for signed range because of as_addi_d
  if (is_intN(imm.value, 12)) {
    as_addi_d(rd, rj, imm.value);
    as_sltui(scratch2, rd, imm.value);
    ma_b(scratch2, scratch2, label,
         cond == Assembler::CarrySet ? Assembler::NonZero : Assembler::Zero);
  } else {
    ma_li(scratch2, imm);
    ma_addPtrTestCarry(cond, rd, rj, scratch2, label);
  }
}

// Subtract.
void MacroAssemblerLOONGARCH64::ma_sub_d(Register rd, Register rj, Imm32 imm) {
  if (is_intN(-imm.value, 12)) {
    as_addi_d(rd, rj, -imm.value);
  } else {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    as_sub_d(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_sub32TestOverflow(Register rd, Register rj,
                                                 Register rk, Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  as_sub_d(scratch, rj, rk);
  as_sub_w(rd, rj, rk);
  ma_b(rd, Register(scratch), overflow, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64::ma_subPtrTestOverflow(Register rd, Register rj,
                                                  Register rk,
                                                  Label* overflow) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT_IF(rj == rd, rj != rk);
  MOZ_ASSERT(rj != scratch2);
  MOZ_ASSERT(rk != scratch2);
  MOZ_ASSERT(rd != scratch2);

  Register rj_copy = rj;

  if (rj == rd) {
    as_or(scratch2, rj, zero);
    rj_copy = scratch2;
  }

  {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rd != scratch);

    as_sub_d(rd, rj, rk);
    // If the sign of rj and rk are the same, no overflow
    as_xor(scratch, rj_copy, rk);
    // Check if the sign of rd and rj are the same
    as_xor(scratch2, rd, rj_copy);
    as_and(scratch2, scratch2, scratch);
  }

  ma_b(scratch2, zero, overflow, Assembler::LessThan);
}

void MacroAssemblerLOONGARCH64::ma_subPtrTestOverflow(Register rd, Register rj,
                                                  Imm32 imm, Label* overflow) {
  // TODO(loongarch64): Check subPtrTestOverflow
  MOZ_ASSERT(imm.value != INT32_MIN);
  ma_addPtrTestOverflow(rd, rj, Imm32(-imm.value), overflow);
}

void MacroAssemblerLOONGARCH64::ma_mul_d(Register rd, Register rj, Imm32 imm) {
  // li handles the relocation.
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(rj != scratch);
  ma_li(scratch, imm);
  as_mul_d(rd, rj, scratch);
}

void MacroAssemblerLOONGARCH64::ma_mulh_d(Register rd, Register rj, Imm32 imm) {
  // li handles the relocation.
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(rj != scratch);
  ma_li(scratch, imm);
  as_mulh_d(rd, rj, scratch);
}

void MacroAssemblerLOONGARCH64::ma_mulPtrTestOverflow(Register rd, Register rj,
                                                  Register rk,
                                                  Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(rd != scratch);

  if (rd == rj) {
    as_or(scratch, rj, zero);
    rj = scratch;
    rk = (rd == rk) ? rj : rk;
  } else if (rd == rk) {
    as_or(scratch, rk, zero);
    rk = scratch;
  }

  as_mul_d(rd, rj, rk);
  as_mulh_d(scratch, rj, rk);
  as_srai_d(scratch2, rd, 63);
  ma_b(scratch, Register(scratch2), overflow, Assembler::NotEqual);
}

// Memory.

void MacroAssemblerLOONGARCH64::ma_load(Register dest, Address address,
                                    LoadStoreSize size,
                                    LoadStoreExtension extension) {
  switch (size) {
    case SizeByte:
      if (ZeroExtend == extension) {
        ma_ld_bu(dest, address);
      } else {
        ma_ld_b(dest, address);
      }
      break;
    case SizeHalfWord:
      if (ZeroExtend == extension) {
        ma_ld_hu(dest, address);
      } else {
        ma_ld_h(dest, address);
      }
      break;
    case SizeWord:
      if ((address.offset & 0x3) == 0 && SignExtend == extension &&
          Imm16::IsInSignedRange(address.offset)) {
        as_ldptr_w(dest, address.base, address.offset);
      } else if (ZeroExtend == extension) {
        ma_ld_wu(dest, address);
      } else {
        ma_ld_w(dest, address);
      }
      break;
    case SizeDouble:
      if ((address.offset & 0x3) == 0 &&
          Imm16::IsInSignedRange(address.offset)) {
        as_ldptr_d(dest, address.base, address.offset);
      } else {
        ma_ld_d(dest, address);
      }
      break;
    default:
      MOZ_CRASH("Invalid argument for ma_load");
  }
}

void MacroAssemblerLOONGARCH64::ma_store(Register data, Address address,
                                     LoadStoreSize size,
                                     LoadStoreExtension extension) {
  switch (size) {
    case SizeByte:
      ma_st_b(data, address);
      break;
    case SizeHalfWord:
      ma_st_h(data, address);
      break;
    case SizeWord:
      if ((address.offset & 0x3) == 0 &&
          Imm16::IsInSignedRange(address.offset)) {
        as_stptr_w(data, address.base, address.offset);
      } else {
        ma_st_w(data, address);
      }
      break;
    case SizeDouble:
      if ((address.offset & 0x3) == 0 &&
          Imm16::IsInSignedRange(address.offset)) {
        as_stptr_d(data, address.base, address.offset);
      } else {
        ma_st_d(data, address);
      }
      break;
    default:
      MOZ_CRASH("Invalid argument for ma_store");
  }
}

void MacroAssemblerLOONGARCH64Compat::computeScaledAddress(const BaseIndex& address,
                                                       Register dest) {
  Register base = address.base;
  Register index = address.index;
  int32_t shift = Imm32::ShiftOf(address.scale).value;

  if (shift) {
    MOZ_ASSERT(shift <= 4);
    as_alsl_d(dest, index, base, shift - 1);
  } else {
    as_add_d(dest, base, index);
  }
}

void MacroAssemblerLOONGARCH64::ma_pop(Register r) {
  MOZ_ASSERT(r != StackPointer);
  as_ld_d(r, StackPointer, 0);
  as_addi_d(StackPointer, StackPointer, sizeof(intptr_t));
}

void MacroAssemblerLOONGARCH64::ma_push(Register r) {
  if (r == StackPointer) {
    ScratchRegisterScope scratch(asMasm());
    as_or(scratch, r, zero);
    as_addi_d(StackPointer, StackPointer, (int32_t) - sizeof(intptr_t));
    as_st_d(scratch, StackPointer, 0);
  } else {
    as_addi_d(StackPointer, StackPointer, (int32_t) - sizeof(intptr_t));
    as_st_d(r, StackPointer, 0);
  }
}

// Branches when done from within loongarch-specific code.
void MacroAssemblerLOONGARCH64::ma_b(Register lhs, ImmWord imm, Label* label,
                                 Condition c, JumpKind jumpKind) {
  if (imm.value <= INT32_MAX) {
    ma_b(lhs, Imm32(uint32_t(imm.value)), label, c, jumpKind);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(lhs != scratch);
    ma_li(scratch, imm);
    ma_b(lhs, Register(scratch), label, c, jumpKind);
  }
}

void MacroAssemblerLOONGARCH64::ma_b(Register lhs, Address addr, Label* label,
                                 Condition c, JumpKind jumpKind) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(lhs != scratch);
  ma_ld_d(scratch, addr);
  ma_b(lhs, Register(scratch), label, c, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_b(Address addr, Imm32 imm, Label* label,
                                 Condition c, JumpKind jumpKind) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_ld_d(scratch2, addr);
  ma_b(Register(scratch2), imm, label, c, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_b(Address addr, ImmGCPtr imm, Label* label,
                                 Condition c, JumpKind jumpKind) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_ld_d(scratch2, addr);
  ma_b(Register(scratch2), imm, label, c, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_bl(Label* label) {
  spew("branch .Llabel %p\n", label);
  if (label->bound()) {
    // Generate the long jump for calls because return address has to be
    // the address after the reserved block.
    addLongJump(nextOffset(), BufferOffset(label->offset()));
    ScratchRegisterScope scratch(asMasm());
    ma_liPatchable(scratch, ImmWord(LabelBase::INVALID_OFFSET));
    as_jirl(ra, scratch, BOffImm16(0));
    return;
  }

  // Second word holds a pointer to the next branch in label's chain.
  uint32_t nextInChain =
      label->used() ? label->offset() : LabelBase::INVALID_OFFSET;

  // Make the whole branch continous in the buffer. The '5'
  // instructions are writing at below.
  m_buffer.ensureSpace(5 * sizeof(uint32_t));

  spew("bal .Llabel %p\n", label);
  BufferOffset bo = writeInst(getBranchCode(BranchIsCall).encode());
  writeInst(nextInChain);
  if (!oom()) {
    label->use(bo.getOffset());
  }
  // Leave space for long jump.
  as_nop();
  as_nop();
  as_nop();
}

void MacroAssemblerLOONGARCH64::branchWithCode(InstImm code, Label* label,
                                           JumpKind jumpKind) {
  // simply output the pointer of one label as its id,
  // notice that after one label destructor, the pointer will be reused.
  spew("branch .Llabel %p", label);
  MOZ_ASSERT(code.encode() !=
             InstImm(op_jirl, BOffImm16(0), zero, ra).encode());
  InstImm inst_beq = InstImm(op_beq, BOffImm16(0), zero, zero);

  if (label->bound()) {
    int32_t offset = label->offset() - m_buffer.nextOffset().getOffset();

    if (BOffImm16::IsInRange(offset)) {
      jumpKind = ShortJump;
    }

    // ShortJump
    if (jumpKind == ShortJump) {
      MOZ_ASSERT(BOffImm16::IsInRange(offset));

      if (code.extractBitField(31, 26) == ((uint32_t)op_bcz >> 26)) {
        code.setImm21(offset);
      } else {
        code.setBOffImm16(BOffImm16(offset));
      }
#ifdef JS_JITSPEW
      decodeBranchInstAndSpew(code);
#endif
      writeInst(code.encode());
      return;
    }

    // LongJump
    if (code.encode() == inst_beq.encode()) {
      // Handle long jump
      addLongJump(nextOffset(), BufferOffset(label->offset()));
      ScratchRegisterScope scratch(asMasm());
      ma_liPatchable(scratch, ImmWord(LabelBase::INVALID_OFFSET));
      as_jirl(zero, scratch, BOffImm16(0));  // jr scratch
      as_nop();
      return;
    }

    // OpenLongJump
    // Handle long conditional branch, the target offset is based on self,
    // point to next instruction of nop at below.
    spew("invert branch .Llabel %p", label);
    InstImm code_r = invertBranch(code, BOffImm16(5 * sizeof(uint32_t)));
#ifdef JS_JITSPEW
    decodeBranchInstAndSpew(code_r);
#endif
    writeInst(code_r.encode());
    addLongJump(nextOffset(), BufferOffset(label->offset()));
    ScratchRegisterScope scratch(asMasm());
    ma_liPatchable(scratch, ImmWord(LabelBase::INVALID_OFFSET));
    as_jirl(zero, scratch, BOffImm16(0));
    as_nop();
    return;
  }

  // Generate open jump and link it to a label.

  // Second word holds a pointer to the next branch in label's chain.
  uint32_t nextInChain =
      label->used() ? label->offset() : LabelBase::INVALID_OFFSET;

  if (jumpKind == ShortJump) {
    // Make the whole branch continous in the buffer.
    m_buffer.ensureSpace(2 * sizeof(uint32_t));

    // Indicate that this is short jump with offset 4.
    code.setBOffImm16(BOffImm16(4));
#ifdef JS_JITSPEW
    decodeBranchInstAndSpew(code);
#endif
    BufferOffset bo = writeInst(code.encode());
    writeInst(nextInChain);
    if (!oom()) {
      label->use(bo.getOffset());
    }
    return;
  }

  bool conditional = code.encode() != inst_beq.encode();

  // Make the whole branch continous in the buffer. The '5'
  // instructions are writing at below (contain conditional nop).
  m_buffer.ensureSpace(5 * sizeof(uint32_t));

#ifdef JS_JITSPEW
  decodeBranchInstAndSpew(code);
#endif
  BufferOffset bo = writeInst(code.encode());  // invert
  writeInst(nextInChain);
  if (!oom()) {
    label->use(bo.getOffset());
  }
  // Leave space for potential long jump.
  as_nop();
  as_nop();
  if (conditional) {
    as_nop();
  }
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Register rj, ImmWord imm,
                                       Condition c) {
  if (imm.value <= INT32_MAX) {
    ma_cmp_set(rd, rj, Imm32(uint32_t(imm.value)), c);
  } else {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    ma_cmp_set(rd, rj, scratch, c);
  }
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Register rj, ImmPtr imm,
                                       Condition c) {
  ma_cmp_set(rd, rj, ImmWord(uintptr_t(imm.value)), c);
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Address address, Imm32 imm,
                                       Condition c) {
  // TODO(loongarch64): 32-bit ma_cmp_set?
  SecondScratchRegisterScope scratch2(asMasm());
  ma_ld_w(scratch2, address);
  ma_cmp_set(rd, Register(scratch2), imm, c);
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Address address,
                                       ImmWord imm, Condition c) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_ld_d(scratch2, address);
  ma_cmp_set(rd, Register(scratch2), imm, c);
}

// fp instructions
void MacroAssemblerLOONGARCH64::ma_lid(FloatRegister dest, double value) {
  ImmWord imm(mozilla::BitwiseCast<uint64_t>(value));

  if (imm.value != 0) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    moveToDouble(scratch, dest);
  } else {
    moveToDouble(zero, dest);
  }
}

void MacroAssemblerLOONGARCH64::ma_mv(FloatRegister src, ValueOperand dest) {
  as_movfr2gr_d(dest.valueReg(), src);
}

void MacroAssemblerLOONGARCH64::ma_mv(ValueOperand src, FloatRegister dest) {
  as_movgr2fr_d(dest, src.valueReg());
}

void MacroAssemblerLOONGARCH64::ma_fld_s(FloatRegister dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_fld_s(dest, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_fldx_s(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_fld_d(FloatRegister dest, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_fld_d(dest, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_fldx_d(dest, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_fst_s(FloatRegister src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_fst_s(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_fstx_s(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_fst_d(FloatRegister src, Address address) {
  int32_t offset = address.offset;
  Register base = address.base;

  if (is_intN(offset, 12)) {
    as_fst_d(src, base, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(base != scratch);
    ma_li(scratch, Imm32(offset));
    as_fstx_d(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_pop(FloatRegister f) {
  if (f.isDouble()) {
    as_fld_d(f, StackPointer, 0);
  } else {
    as_fld_s(f, StackPointer, 0);
  }
  as_addi_d(StackPointer, StackPointer, f.size());
}

void MacroAssemblerLOONGARCH64::ma_push(FloatRegister f) {
  as_addi_d(StackPointer, StackPointer, -int32_t(f.size()));
  if (f.isDouble()) {
    as_fst_d(f, StackPointer, 0);
  } else {
    as_fst_s(f, StackPointer, 0);
  }
}

void MacroAssemblerLOONGARCH64::ma_li(Register dest, ImmGCPtr ptr) {
  writeDataRelocation(ptr);
  asMasm().ma_liPatchable(dest, ImmPtr(ptr.value));
}

void MacroAssemblerLOONGARCH64::ma_li(Register dest, Imm32 imm) {
  if (is_intN(imm.value, 12)) {
    as_addi_w(dest, zero, imm.value);
  } else if (is_uintN(imm.value, 12)) {
    as_ori(dest, zero, imm.value & 0xfff);
  } else {
    as_lu12i_w(dest, imm.value >> 12 & 0xfffff);
    if (imm.value & 0xfff) {
      as_ori(dest, dest, imm.value & 0xfff);
    }
  }
}

// This method generates lu12i_w and ori instruction pair that can be modified
// by UpdateLuiOriValue, either during compilation (eg. Assembler::bind), or
// during execution (eg. jit::PatchJump).
void MacroAssemblerLOONGARCH64::ma_liPatchable(Register dest, Imm32 imm) {
  m_buffer.ensureSpace(2 * sizeof(uint32_t));
  as_lu12i_w(dest, imm.value >> 12 & 0xfffff);
  as_ori(dest, dest, imm.value & 0xfff);
}

void MacroAssemblerLOONGARCH64::ma_fmovz(FloatFormat fmt, FloatRegister fd,
                                     FloatRegister fj, Register rk) {
  Label done;
  ma_b(rk, zero, &done, Assembler::NotEqual);
  if (fmt == SingleFloat) {
    as_fmov_s(fd, fj);
  } else {
    as_fmov_d(fd, fj);
  }
  bind(&done);
}

void MacroAssemblerLOONGARCH64::ma_fmovn(FloatFormat fmt, FloatRegister fd,
                                     FloatRegister fj, Register rk) {
  Label done;
  ma_b(rk, zero, &done, Assembler::Equal);
  if (fmt == SingleFloat) {
    as_fmov_s(fd, fj);
  } else {
    as_fmov_d(fd, fj);
  }
  bind(&done);
}

void MacroAssemblerLOONGARCH64::ma_and(Register rd, Register rj, Imm32 imm,
                                   bool bit32) {
  if (is_uintN(imm.value, 12)) {
    as_andi(rd, rj, imm.value);
  } else if (rd != rj) {
    ma_li(rd, imm);
    as_and(rd, rj, rd);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_and(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_or(Register rd, Register rj, Imm32 imm,
                                  bool bit32) {
  if (is_uintN(imm.value, 12)) {
    as_ori(rd, rj, imm.value);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_or(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_xor(Register rd, Register rj, Imm32 imm,
                                   bool bit32) {
  if (is_uintN(imm.value, 12)) {
    as_xori(rd, rj, imm.value);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_xor(rd, rj, scratch);
  }
}

// Arithmetic-based ops.

// Add.
void MacroAssemblerLOONGARCH64::ma_add_w(Register rd, Register rj, Imm32 imm) {
  if (is_intN(imm.value, 12)) {
    as_addi_w(rd, rj, imm.value);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_add_w(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_add32TestCarry(Condition cond, Register rd,
                                              Register rj, Register rk,
                                              Label* overflow) {
  MOZ_ASSERT(cond == Assembler::CarrySet || cond == Assembler::CarryClear);
  MOZ_ASSERT_IF(rd == rj, rk != rd);
  ScratchRegisterScope scratch(asMasm());
  as_add_w(rd, rj, rk);
  as_sltu(scratch, rd, rd == rj ? rk : rj);
  ma_b(Register(scratch), Register(scratch), overflow,
       cond == Assembler::CarrySet ? Assembler::NonZero : Assembler::Zero);
}

void MacroAssemblerLOONGARCH64::ma_add32TestCarry(Condition cond, Register rd,
                                              Register rj, Imm32 imm,
                                              Label* overflow) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(rj != scratch2);
  ma_li(scratch2, imm);
  ma_add32TestCarry(cond, rd, rj, scratch2, overflow);
}

// Subtract.
void MacroAssemblerLOONGARCH64::ma_sub_w(Register rd, Register rj, Imm32 imm) {
  if (is_intN(-imm.value, 12)) {
    as_addi_w(rd, rj, -imm.value);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, imm);
    as_sub_w(rd, rj, scratch);
  }
}

void MacroAssemblerLOONGARCH64::ma_sub_w(Register rd, Register rj, Register rk) {
  as_sub_w(rd, rj, rk);
}

void MacroAssemblerLOONGARCH64::ma_sub32TestOverflow(Register rd, Register rj,
                                                 Imm32 imm, Label* overflow) {
  if (imm.value != INT32_MIN) {
    asMasm().ma_add32TestOverflow(rd, rj, Imm32(-imm.value), overflow);
  } else {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rj != scratch);
    ma_li(scratch, Imm32(imm.value));
    asMasm().ma_sub32TestOverflow(rd, rj, scratch, overflow);
  }
}

void MacroAssemblerLOONGARCH64::ma_mul(Register rd, Register rj, Imm32 imm) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(rj != scratch);
  ma_li(scratch, imm);
  as_mul_w(rd, rj, scratch);
}

void MacroAssemblerLOONGARCH64::ma_mul32TestOverflow(Register rd, Register rj,
                                                 Register rk, Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  as_mulh_w(scratch, rj, rk);
  as_mul_w(rd, rj, rk);
  as_srai_w(scratch2, rd, 31);
  ma_b(scratch, Register(scratch2), overflow, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64::ma_mul32TestOverflow(Register rd, Register rj,
                                                 Imm32 imm, Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch, imm);
  as_mulh_w(scratch2, rj, scratch);
  as_mul_w(rd, rj, scratch);
  as_srai_w(scratch, rd, 31);
  ma_b(scratch, Register(scratch2), overflow, Assembler::NotEqual);
}

void MacroAssemblerLOONGARCH64::ma_div_branch_overflow(Register rd, Register rj,
                                                   Register rk,
                                                   Label* overflow) {
  ScratchRegisterScope scratch(asMasm());
  as_mod_w(scratch, rj, rk);
  ma_b(scratch, scratch, overflow, Assembler::NonZero);
  as_div_w(rd, rj, rk);
}

void MacroAssemblerLOONGARCH64::ma_div_branch_overflow(Register rd, Register rj,
                                                   Imm32 imm, Label* overflow) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch2, imm);
  ma_div_branch_overflow(rd, rj, scratch2, overflow);
}

void MacroAssemblerLOONGARCH64::ma_mod_mask(Register src, Register dest,
                                        Register hold, Register remain,
                                        int32_t shift, Label* negZero) {
  // MATH:
  // We wish to compute x % (1<<y) - 1 for a known constant, y.
  // First, let b = (1<<y) and C = (1<<y)-1, then think of the 32 bit
  // dividend as a number in base b, namely
  // c_0*1 + c_1*b + c_2*b^2 ... c_n*b^n
  // now, since both addition and multiplication commute with modulus,
  // x % C == (c_0 + c_1*b + ... + c_n*b^n) % C ==
  // (c_0 % C) + (c_1%C) * (b % C) + (c_2 % C) * (b^2 % C)...
  // now, since b == C + 1, b % C == 1, and b^n % C == 1
  // this means that the whole thing simplifies to:
  // c_0 + c_1 + c_2 ... c_n % C
  // each c_n can easily be computed by a shift/bitextract, and the modulus
  // can be maintained by simply subtracting by C whenever the number gets
  // over C.
  int32_t mask = (1 << shift) - 1;
  Label head, negative, sumSigned, done;

  // hold holds -1 if the value was negative, 1 otherwise.
  // remain holds the remaining bits that have not been processed
  // SecondScratchReg serves as a temporary location to store extracted bits
  // into as well as holding the trial subtraction as a temp value dest is
  // the accumulator (and holds the final result)

  // move the whole value into the remain.
  as_or(remain, src, zero);
  // Zero out the dest.
  ma_li(dest, Imm32(0));
  // Set the hold appropriately.
  ma_b(remain, remain, &negative, Signed, ShortJump);
  ma_li(hold, Imm32(1));
  ma_b(&head, ShortJump);

  bind(&negative);
  ma_li(hold, Imm32(-1));
  as_sub_w(remain, zero, remain);

  // Begin the main loop.
  bind(&head);

  SecondScratchRegisterScope scratch2(asMasm());
  // Extract the bottom bits into SecondScratchReg.
  ma_and(scratch2, remain, Imm32(mask));
  // Add those bits to the accumulator.
  as_add_w(dest, dest, scratch2);
  // Do a trial subtraction
  ma_sub_w(scratch2, dest, Imm32(mask));
  // If (sum - C) > 0, store sum - C back into sum, thus performing a
  // modulus.
  ma_b(scratch2, Register(scratch2), &sumSigned, Signed, ShortJump);
  as_or(dest, scratch2, zero);
  bind(&sumSigned);
  // Get rid of the bits that we extracted before.
  as_srli_w(remain, remain, shift);
  // If the shift produced zero, finish, otherwise, continue in the loop.
  ma_b(remain, remain, &head, NonZero, ShortJump);
  // Check the hold to see if we need to negate the result.
  ma_b(hold, hold, &done, NotSigned, ShortJump);

  // If the hold was non-zero, negate the result to be in line with
  // what JS wants
  if (negZero != nullptr) {
    // Jump out in case of negative zero.
    ma_b(hold, hold, negZero, Zero);
    as_sub_w(dest, zero, dest);
  } else {
    as_sub_w(dest, zero, dest);
  }

  bind(&done);
}

// Memory.

void MacroAssemblerLOONGARCH64::ma_load(Register dest, const BaseIndex& src,
                                    LoadStoreSize size,
                                    LoadStoreExtension extension) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(src, scratch2);
  asMasm().ma_load(dest, Address(scratch2, src.offset), size, extension);
}

void MacroAssemblerLOONGARCH64::ma_store(Register data, const BaseIndex& dest,
                                     LoadStoreSize size,
                                     LoadStoreExtension extension) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(dest, scratch2);
  asMasm().ma_store(data, Address(scratch2, dest.offset), size, extension);
}

void MacroAssemblerLOONGARCH64::ma_store(Imm32 imm, const BaseIndex& dest,
                                     LoadStoreSize size,
                                     LoadStoreExtension extension) {
  SecondScratchRegisterScope scratch2(asMasm());
  // Make sure that scratch2 contains absolute address so that offset is 0.
  asMasm().computeEffectiveAddress(dest, scratch2);

  ScratchRegisterScope scratch(asMasm());
  // Scrach register is free now, use it for loading imm value
  ma_li(scratch, imm);

  // with offset=0 ScratchRegister will not be used in ma_store()
  // so we can use it as a parameter here
  asMasm().ma_store(scratch, Address(scratch2, 0), size, extension);
}

void MacroAssemblerLOONGARCH64::ma_load_unaligned(
    const wasm::MemoryAccessDesc& access, Register dest, const BaseIndex& src,
    Register temp, LoadStoreSize size, LoadStoreExtension extension) {
  MOZ_ASSERT(dest != temp);

  asMasm().computeEffectiveAddress(src, temp);

  BufferOffset load = as_ld_bu(dest, temp, 0);
  if (size != SizeByte) {
    ScratchRegisterScope scratch(asMasm());
    const size_t byteSize = size / 8;
    for (size_t i = 1; i < byteSize; i++) {
      as_ld_bu(scratch, temp, i);
      as_slli_d(scratch, scratch, 8 * i);
      as_or(dest, dest, scratch);
    }
  }

  switch (size) {
    case SizeByte:
      if (extension == SignExtend) {
        as_ext_w_b(dest, dest);
      }
      break;
    case SizeHalfWord:
      if (extension == SignExtend) {
        as_ext_w_h(dest, dest);
      }
      break;
    case SizeWord:
      if (extension == SignExtend) {
        as_addi_w(dest, dest, 0);
      }
      break;
    case SizeDouble:
      break;
    default:
      MOZ_CRASH("Invalid argument for ma_load_unaligned");
  }

  append(access, load.getOffset(), asMasm().framePushed());
}

void MacroAssemblerLOONGARCH64::ma_store_unaligned(
    const wasm::MemoryAccessDesc& access, Register data, const BaseIndex& dest,
    Register temp, LoadStoreSize size, LoadStoreExtension extension) {
  (void)extension;

  MOZ_ASSERT(data != temp);

  asMasm().computeEffectiveAddress(dest, temp);

  BufferOffset store = as_st_b(data, temp, 0);
  if (size != SizeByte) {
    ScratchRegisterScope scratch(asMasm());
    const size_t byteSize = size / 8;
    for (size_t i = 1; i < byteSize; i++) {
      as_srli_d(scratch, data, 8 * i);
      as_st_b(scratch, temp, i);
    }
  }

  append(access, store.getOffset(), asMasm().framePushed());
}

void MacroAssemblerLOONGARCH64::loadUnalignedDouble(
    const wasm::MemoryAccessDesc& access, const BaseIndex& src, Register temp,
    FloatRegister dest) {
  asMasm().computeEffectiveAddress(src, temp);

  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());

  BufferOffset load = as_ld_bu(scratch, temp, 0);
  for (size_t i = 1; i < sizeof(double); i++) {
    as_ld_bu(scratch2, temp, i);
    as_slli_d(scratch2, scratch2, 8 * i);
    as_or(scratch, scratch, scratch2);
  }

  append(access, load.getOffset(), asMasm().framePushed());
  moveToDouble(scratch, dest);
}

void MacroAssemblerLOONGARCH64::loadUnalignedFloat32(
    const wasm::MemoryAccessDesc& access, const BaseIndex& src, Register temp,
    FloatRegister dest) {
  asMasm().computeEffectiveAddress(src, temp);

  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());

  BufferOffset load = as_ld_bu(scratch, temp, 0);
  for (size_t i = 1; i < sizeof(float); i++) {
    as_ld_bu(scratch2, temp, i);
    as_slli_d(scratch2, scratch2, 8 * i);
    as_or(scratch, scratch, scratch2);
  }

  append(access, load.getOffset(), asMasm().framePushed());
  moveToFloat32(scratch, dest);
}

void MacroAssemblerLOONGARCH64::storeUnalignedDouble(
    const wasm::MemoryAccessDesc& access, FloatRegister src, Register temp,
    const BaseIndex& dest) {
  asMasm().computeEffectiveAddress(dest, temp);

  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  moveFromDouble(src, scratch);

  BufferOffset store = as_st_b(scratch, temp, 0);
  for (size_t i = 1; i < sizeof(double); i++) {
    as_srli_d(scratch2, scratch, 8 * i);
    as_st_b(scratch2, temp, i);
  }

  append(access, store.getOffset(), asMasm().framePushed());
}

void MacroAssemblerLOONGARCH64::storeUnalignedFloat32(
    const wasm::MemoryAccessDesc& access, FloatRegister src, Register temp,
    const BaseIndex& dest) {
  asMasm().computeEffectiveAddress(dest, temp);

  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  moveFromFloat32(src, scratch);

  BufferOffset store = as_st_b(scratch, temp, 0);
  for (size_t i = 1; i < sizeof(float); i++) {
    as_srli_d(scratch2, scratch, 8 * i);
    as_st_b(scratch2, temp, i);
  }

  append(access, store.getOffset(), asMasm().framePushed());
}

// Branches when done from within loongarch-specific code.
// TODO(loongarch64) Optimize ma_b
void MacroAssemblerLOONGARCH64::ma_b(Register lhs, Register rhs, Label* label,
                                 Condition c, JumpKind jumpKind) {
  switch (c) {
    case Equal:
    case NotEqual:
      asMasm().branchWithCode(getBranchCode(lhs, rhs, c), label, jumpKind);
      break;
    case Always:
      ma_b(label, jumpKind);
      break;
    case Zero:
    case NonZero:
    case Signed:
    case NotSigned:
      MOZ_ASSERT(lhs == rhs);
      asMasm().branchWithCode(getBranchCode(lhs, c), label, jumpKind);
      break;
    default: {
      Condition cond = ma_cmp(ScratchRegister, lhs, rhs, c);
      asMasm().branchWithCode(getBranchCode(ScratchRegister, cond), label,
                              jumpKind);
      break;
    }
  }
}

void MacroAssemblerLOONGARCH64::ma_b(Register lhs, Imm32 imm, Label* label,
                                 Condition c, JumpKind jumpKind) {
  MOZ_ASSERT(c != Overflow);
  if (imm.value == 0) {
    if (c == Always || c == AboveOrEqual) {
      ma_b(label, jumpKind);
    } else if (c == Below) {
      ;  // This condition is always false. No branch required.
    } else {
      asMasm().branchWithCode(getBranchCode(lhs, c), label, jumpKind);
    }
  } else {
    switch (c) {
      case Equal:
      case NotEqual:
        MOZ_ASSERT(lhs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_b(lhs, ScratchRegister, label, c, jumpKind);
        break;
      default:
        Condition cond = ma_cmp(ScratchRegister, lhs, imm, c);
        asMasm().branchWithCode(getBranchCode(ScratchRegister, cond), label,
                                jumpKind);
    }
  }
}

void MacroAssemblerLOONGARCH64::ma_b(Register lhs, ImmPtr imm, Label* l,
                                 Condition c, JumpKind jumpKind) {
  asMasm().ma_b(lhs, ImmWord(uintptr_t(imm.value)), l, c, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_b(Label* label, JumpKind jumpKind) {
  asMasm().branchWithCode(getBranchCode(BranchIsJump), label, jumpKind);
}

Assembler::Condition MacroAssemblerLOONGARCH64::ma_cmp(Register dest, Register lhs,
                                                   Register rhs, Condition c) {
  switch (c) {
    case Above:
      // bgtu s,t,label =>
      //   sltu at,t,s
      //   bne at,$zero,offs
      as_sltu(dest, rhs, lhs);
      return NotEqual;
    case AboveOrEqual:
      // bgeu s,t,label =>
      //   sltu at,s,t
      //   beq at,$zero,offs
      as_sltu(dest, lhs, rhs);
      return Equal;
    case Below:
      // bltu s,t,label =>
      //   sltu at,s,t
      //   bne at,$zero,offs
      as_sltu(dest, lhs, rhs);
      return NotEqual;
    case BelowOrEqual:
      // bleu s,t,label =>
      //   sltu at,t,s
      //   beq at,$zero,offs
      as_sltu(dest, rhs, lhs);
      return Equal;
    case GreaterThan:
      // bgt s,t,label =>
      //   slt at,t,s
      //   bne at,$zero,offs
      as_slt(dest, rhs, lhs);
      return NotEqual;
    case GreaterThanOrEqual:
      // bge s,t,label =>
      //   slt at,s,t
      //   beq at,$zero,offs
      as_slt(dest, lhs, rhs);
      return Equal;
    case LessThan:
      // blt s,t,label =>
      //   slt at,s,t
      //   bne at,$zero,offs
      as_slt(dest, lhs, rhs);
      return NotEqual;
    case LessThanOrEqual:
      // ble s,t,label =>
      //   slt at,t,s
      //   beq at,$zero,offs
      as_slt(dest, rhs, lhs);
      return Equal;
    default:
      MOZ_CRASH("Invalid condition.");
  }
  return Always;
}

Assembler::Condition MacroAssemblerLOONGARCH64::ma_cmp(Register dest, Register lhs,
                                                   Imm32 imm, Condition c) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_RELEASE_ASSERT(lhs != scratch);

  switch (c) {
    case Above:
    case BelowOrEqual:
      if (imm.value != 0x7fffffff && is_intN(imm.value + 1, 12) &&
          imm.value != -1) {
        // lhs <= rhs via lhs < rhs + 1 if rhs + 1 does not overflow
        as_sltui(dest, lhs, imm.value + 1);

        return (c == BelowOrEqual ? NotEqual : Equal);
      } else {
        ma_li(scratch, imm);
        as_sltu(dest, scratch, lhs);
        return (c == BelowOrEqual ? Equal : NotEqual);
      }
    case AboveOrEqual:
    case Below:
      if (is_intN(imm.value, 12)) {
        as_sltui(dest, lhs, imm.value);
      } else {
        ma_li(scratch, imm);
        as_sltu(dest, lhs, scratch);
      }
      return (c == AboveOrEqual ? Equal : NotEqual);
    case GreaterThan:
    case LessThanOrEqual:
      if (imm.value != 0x7fffffff && is_intN(imm.value + 1, 12)) {
        // lhs <= rhs via lhs < rhs + 1.
        as_slti(dest, lhs, imm.value + 1);
        return (c == LessThanOrEqual ? NotEqual : Equal);
      } else {
        ma_li(scratch, imm);
        as_slt(dest, scratch, lhs);
        return (c == LessThanOrEqual ? Equal : NotEqual);
      }
    case GreaterThanOrEqual:
    case LessThan:
      if (is_intN(imm.value, 12)) {
        as_slti(dest, lhs, imm.value);
      } else {
        ma_li(scratch, imm);
        as_slt(dest, lhs, scratch);
      }
      return (c == GreaterThanOrEqual ? Equal : NotEqual);
    default:
      MOZ_CRASH("Invalid condition.");
  }
  return Always;
}

// fp instructions
void MacroAssemblerLOONGARCH64::ma_lis(FloatRegister dest, float value) {
  Imm32 imm(mozilla::BitwiseCast<uint32_t>(value));

  if (imm.value != 0) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    moveToFloat32(scratch, dest);
  } else {
    moveToFloat32(zero, dest);
  }
}

void MacroAssemblerLOONGARCH64::ma_fst_d(FloatRegister ft, BaseIndex address) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(address, scratch2);
  asMasm().ma_fst_d(ft, Address(scratch2, address.offset));
}

void MacroAssemblerLOONGARCH64::ma_fst_s(FloatRegister ft, BaseIndex address) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(address, scratch2);
  asMasm().ma_fst_s(ft, Address(scratch2, address.offset));
}

void MacroAssemblerLOONGARCH64::ma_fld_d(FloatRegister ft, const BaseIndex& src) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(src, scratch2);
  asMasm().ma_fld_d(ft, Address(scratch2, src.offset));
}

void MacroAssemblerLOONGARCH64::ma_fld_s(FloatRegister ft, const BaseIndex& src) {
  SecondScratchRegisterScope scratch2(asMasm());
  asMasm().computeScaledAddress(src, scratch2);
  asMasm().ma_fld_s(ft, Address(scratch2, src.offset));
}

void MacroAssemblerLOONGARCH64::ma_bc_s(FloatRegister lhs, FloatRegister rhs,
                                    Label* label, DoubleCondition c,
                                    JumpKind jumpKind, FPConditionBit fcc) {
  compareFloatingPoint(SingleFloat, lhs, rhs, c, fcc);
  asMasm().branchWithCode(getBranchCode(fcc), label, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_bc_d(FloatRegister lhs, FloatRegister rhs,
                                    Label* label, DoubleCondition c,
                                    JumpKind jumpKind, FPConditionBit fcc) {
  compareFloatingPoint(DoubleFloat, lhs, rhs, c, fcc);
  asMasm().branchWithCode(getBranchCode(fcc), label, jumpKind);
}

void MacroAssemblerLOONGARCH64::ma_call(ImmPtr dest) {
  asMasm().ma_liPatchable(CallReg, dest);
  as_jirl(ra, CallReg, BOffImm16(0));
}

void MacroAssemblerLOONGARCH64::ma_jump(ImmPtr dest) {
  ScratchRegisterScope scratch(asMasm());
  asMasm().ma_liPatchable(scratch, dest);
  as_jirl(zero, scratch, BOffImm16(0));
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Register rj, Register rk,
                                       Condition c) {
  switch (c) {
    case Equal:
      // seq d,s,t =>
      //   xor d,s,t
      //   sltiu d,d,1
      as_xor(rd, rj, rk);
      as_sltui(rd, rd, 1);
      break;
    case NotEqual:
      // sne d,s,t =>
      //   xor d,s,t
      //   sltu d,$zero,d
      as_xor(rd, rj, rk);
      as_sltu(rd, zero, rd);
      break;
    case Above:
      // sgtu d,s,t =>
      //   sltu d,t,s
      as_sltu(rd, rk, rj);
      break;
    case AboveOrEqual:
      // sgeu d,s,t =>
      //   sltu d,s,t
      //   xori d,d,1
      as_sltu(rd, rj, rk);
      as_xori(rd, rd, 1);
      break;
    case Below:
      // sltu d,s,t
      as_sltu(rd, rj, rk);
      break;
    case BelowOrEqual:
      // sleu d,s,t =>
      //   sltu d,t,s
      //   xori d,d,1
      as_sltu(rd, rk, rj);
      as_xori(rd, rd, 1);
      break;
    case GreaterThan:
      // sgt d,s,t =>
      //   slt d,t,s
      as_slt(rd, rk, rj);
      break;
    case GreaterThanOrEqual:
      // sge d,s,t =>
      //   slt d,s,t
      //   xori d,d,1
      as_slt(rd, rj, rk);
      as_xori(rd, rd, 1);
      break;
    case LessThan:
      // slt d,s,t
      as_slt(rd, rj, rk);
      break;
    case LessThanOrEqual:
      // sle d,s,t =>
      //   slt d,t,s
      //   xori d,d,1
      as_slt(rd, rk, rj);
      as_xori(rd, rd, 1);
      break;
    case Zero:
      MOZ_ASSERT(rj == rk);
      // seq d,s,$zero =>
      //   sltiu d,s,1
      as_sltui(rd, rj, 1);
      break;
    case NonZero:
      MOZ_ASSERT(rj == rk);
      // sne d,s,$zero =>
      //   sltu d,$zero,s
      as_sltu(rd, zero, rj);
      break;
    case Signed:
      MOZ_ASSERT(rj == rk);
      as_slt(rd, rj, zero);
      break;
    case NotSigned:
      MOZ_ASSERT(rj == rk);
      // sge d,s,$zero =>
      //   slt d,s,$zero
      //   xori d,d,1
      as_slt(rd, rj, zero);
      as_xori(rd, rd, 1);
      break;
    default:
      MOZ_CRASH("Invalid condition.");
  }
}

void MacroAssemblerLOONGARCH64::ma_cmp_set_double(Register dest, FloatRegister lhs,
                                              FloatRegister rhs,
                                              DoubleCondition c) {
  compareFloatingPoint(DoubleFloat, lhs, rhs, c);
  as_movcf2gr(dest, FCC0);
}

void MacroAssemblerLOONGARCH64::ma_cmp_set_float32(Register dest, FloatRegister lhs,
                                               FloatRegister rhs,
                                               DoubleCondition c) {
  compareFloatingPoint(SingleFloat, lhs, rhs, c);
  as_movcf2gr(dest, FCC0);
}

void MacroAssemblerLOONGARCH64::ma_cmp_set(Register rd, Register rj, Imm32 imm,
                                       Condition c) {
  if (imm.value == 0) {
    switch (c) {
      case Equal:
      case BelowOrEqual:
        as_sltui(rd, rj, 1);
        break;
      case NotEqual:
      case Above:
        as_sltu(rd, zero, rj);
        break;
      case AboveOrEqual:
      case Below:
        as_ori(rd, zero, c == AboveOrEqual ? 1 : 0);
        break;
      case GreaterThan:
      case LessThanOrEqual:
        as_slt(rd, zero, rj);
        if (c == LessThanOrEqual) {
          as_xori(rd, rd, 1);
        }
        break;
      case LessThan:
      case GreaterThanOrEqual:
        as_slt(rd, rj, zero);
        if (c == GreaterThanOrEqual) {
          as_xori(rd, rd, 1);
        }
        break;
      case Zero:
        as_sltui(rd, rj, 1);
        break;
      case NonZero:
        as_sltu(rd, zero, rj);
        break;
      case Signed:
        as_slt(rd, rj, zero);
        break;
      case NotSigned:
        as_slt(rd, rj, zero);
        as_xori(rd, rd, 1);
        break;
      default:
        MOZ_CRASH("Invalid condition.");
    }
    return;
  }

  switch (c) {
    case Equal:
    case NotEqual:
      ma_xor(rd, rj, imm);
      if (c == Equal) {
        as_sltui(rd, rd, 1);
      } else {
        as_sltu(rd, zero, rd);
      }
      break;
    case Zero:
    case NonZero:
    case Signed:
    case NotSigned:
      MOZ_CRASH("Invalid condition.");
    default:
      Condition cond = ma_cmp(rd, rj, imm, c);
      MOZ_ASSERT(cond == Equal || cond == NotEqual);

      if (cond == Equal) as_xori(rd, rd, 1);
  }
}

void MacroAssemblerLOONGARCH64::compareFloatingPoint(FloatFormat fmt,
                                                 FloatRegister lhs,
                                                 FloatRegister rhs,
                                                 DoubleCondition c,
                                                 FPConditionBit fcc) {
  switch (c) {
    case DoubleOrdered:
      as_fcmp_cor(fmt, lhs, rhs, fcc);
      break;
    case DoubleEqual:
      as_fcmp_ceq(fmt, lhs, rhs, fcc);
      break;
    case DoubleNotEqual:
      as_fcmp_cne(fmt, lhs, rhs, fcc);
      break;
    case DoubleGreaterThan:
      as_fcmp_clt(fmt, rhs, lhs, fcc);
      break;
    case DoubleGreaterThanOrEqual:
      as_fcmp_cle(fmt, rhs, lhs, fcc);
      break;
    case DoubleLessThan:
      as_fcmp_clt(fmt, lhs, rhs, fcc);
      break;
    case DoubleLessThanOrEqual:
      as_fcmp_cle(fmt, lhs, rhs, fcc);
      break;
    case DoubleUnordered:
      as_fcmp_cun(fmt, lhs, rhs, fcc);
      break;
    case DoubleEqualOrUnordered:
      as_fcmp_cueq(fmt, lhs, rhs, fcc);
      break;
    case DoubleNotEqualOrUnordered:
      as_fcmp_cune(fmt, lhs, rhs, fcc);
      break;
    case DoubleGreaterThanOrUnordered:
      as_fcmp_cult(fmt, rhs, lhs, fcc);
      break;
    case DoubleGreaterThanOrEqualOrUnordered:
      as_fcmp_cule(fmt, rhs, lhs, fcc);
      break;
    case DoubleLessThanOrUnordered:
      as_fcmp_cult(fmt, lhs, rhs, fcc);
      break;
    case DoubleLessThanOrEqualOrUnordered:
      as_fcmp_cule(fmt, lhs, rhs, fcc);
      break;
    default:
      MOZ_CRASH("Invalid DoubleCondition.");
  }
}

void MacroAssemblerLOONGARCH64::minMaxDouble(FloatRegister srcDest,
                                         FloatRegister second, bool handleNaN,
                                         bool isMax) {
  if (srcDest == second) return;

  Label nan, done;

  // First or second is NaN, result is NaN.
  ma_bc_d(srcDest, second, &nan, Assembler::DoubleUnordered, ShortJump);
  if (isMax) {
    as_fmax_d(srcDest, srcDest, second);
  } else {
    as_fmin_d(srcDest, srcDest, second);
  }
  ma_b(&done, ShortJump);

  bind(&nan);
  as_fadd_d(srcDest, srcDest, second);

  bind(&done);
}

void MacroAssemblerLOONGARCH64::minMaxFloat32(FloatRegister srcDest,
                                          FloatRegister second, bool handleNaN,
                                          bool isMax) {
  if (srcDest == second) return;

  Label nan, done;

  // First or second is NaN, result is NaN.
  ma_bc_s(srcDest, second, &nan, Assembler::DoubleUnordered, ShortJump);
  if (isMax) {
    as_fmax_s(srcDest, srcDest, second);
  } else {
    as_fmin_s(srcDest, srcDest, second);
  }
  ma_b(&done, ShortJump);

  bind(&nan);
  as_fadd_s(srcDest, srcDest, second);

  bind(&done);
}

void MacroAssemblerLOONGARCH64::loadDouble(const Address& address,
                                       FloatRegister dest) {
  asMasm().ma_fld_d(dest, address);
}

void MacroAssemblerLOONGARCH64::loadDouble(const BaseIndex& src,
                                       FloatRegister dest) {
  asMasm().ma_fld_d(dest, src);
}

void MacroAssemblerLOONGARCH64::loadFloatAsDouble(const Address& address,
                                              FloatRegister dest) {
  asMasm().ma_fld_s(dest, address);
  as_fcvt_d_s(dest, dest);
}

void MacroAssemblerLOONGARCH64::loadFloatAsDouble(const BaseIndex& src,
                                              FloatRegister dest) {
  asMasm().loadFloat32(src, dest);
  as_fcvt_d_s(dest, dest);
}

void MacroAssemblerLOONGARCH64::loadFloat32(const Address& address,
                                        FloatRegister dest) {
  asMasm().ma_fld_s(dest, address);
}

void MacroAssemblerLOONGARCH64::loadFloat32(const BaseIndex& src,
                                        FloatRegister dest) {
  asMasm().ma_fld_s(dest, src);
}

void MacroAssemblerLOONGARCH64::wasmLoadImpl(const wasm::MemoryAccessDesc& access,
                                         Register memoryBase, Register ptr,
                                         Register ptrScratch,
                                         AnyRegister output, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrier(access.barrierBefore());

  switch (access.type()) {
    case Scalar::Int8:
      as_ldx_b(output.gpr(), memoryBase, ptr);
      break;
    case Scalar::Uint8:
      as_ldx_bu(output.gpr(), memoryBase, ptr);
      break;
    case Scalar::Int16:
      as_ldx_h(output.gpr(), memoryBase, ptr);
      break;
    case Scalar::Uint16:
      as_ldx_hu(output.gpr(), memoryBase, ptr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      as_ldx_w(output.gpr(), memoryBase, ptr);
      break;
    case Scalar::Float64:
      as_fldx_d(output.fpu(), memoryBase, ptr);
      break;
    case Scalar::Float32:
      as_fldx_s(output.fpu(), memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  asMasm().append(access, asMasm().size() - 4, asMasm().framePushed());
  asMasm().memoryBarrier(access.barrierAfter());
}

void MacroAssemblerLOONGARCH64::wasmStoreImpl(const wasm::MemoryAccessDesc& access,
                                          AnyRegister value,
                                          Register memoryBase, Register ptr,
                                          Register ptrScratch, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrier(access.barrierBefore());

  switch (access.type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
      as_stx_b(value.gpr(), memoryBase, ptr);
      break;
    case Scalar::Int16:
    case Scalar::Uint16:
      as_stx_h(value.gpr(), memoryBase, ptr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      as_stx_w(value.gpr(), memoryBase, ptr);
      break;
    case Scalar::Int64:
      as_stx_d(value.gpr(), memoryBase, ptr);
      break;
    case Scalar::Float64:
      as_fstx_d(value.fpu(), memoryBase, ptr);
      break;
    case Scalar::Float32:
      as_fstx_s(value.fpu(), memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  // Only the last emitted instruction is a memory access.
  asMasm().append(access, asMasm().size() - 4, asMasm().framePushed());
  asMasm().memoryBarrier(access.barrierAfter());
}

void MacroAssemblerLOONGARCH64Compat::wasmLoadI64Impl(
    const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
    Register ptrScratch, Register64 output, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrier(access.barrierBefore());

  switch (access.type()) {
    case Scalar::Int8:
      as_ldx_b(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint8:
      as_ldx_bu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int16:
      as_ldx_h(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint16:
      as_ldx_hu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int32:
      as_ldx_w(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint32:
      // TODO(loongarch64): Why need zero-extension here?
      as_ldx_wu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int64:
      as_ldx_d(output.reg, memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  asMasm().append(access, asMasm().size() - 4, asMasm().framePushed());
  asMasm().memoryBarrier(access.barrierAfter());
}

void MacroAssemblerLOONGARCH64Compat::wasmStoreI64Impl(
    const wasm::MemoryAccessDesc& access, Register64 value, Register memoryBase,
    Register ptr, Register ptrScratch, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrier(access.barrierBefore());

  switch (access.type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
      as_stx_b(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int16:
    case Scalar::Uint16:
      as_stx_h(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      as_stx_w(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int64:
      as_stx_d(value.reg, memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  asMasm().append(access, asMasm().size() - 4, asMasm().framePushed());
  asMasm().memoryBarrier(access.barrierAfter());
}

void MacroAssemblerLOONGARCH64Compat::profilerEnterFrame(Register framePtr,
                                                     Register scratch) {
  AbsoluteAddress activation(GetJitContext()->runtime->addressOfProfilingActivation());
  loadPtr(activation, scratch);
  storePtr(framePtr,
           Address(scratch, JitActivation::offsetOfLastProfilingFrame()));
  storePtr(ImmPtr(nullptr),
           Address(scratch, JitActivation::offsetOfLastProfilingCallSite()));
}

void MacroAssemblerLOONGARCH64Compat::profilerExitFrame() {
  jump(GetJitContext()->runtime->jitRuntime()->getProfilerExitFrameTail());
}

MacroAssembler& MacroAssemblerLOONGARCH64::asMasm() {
  return *static_cast<MacroAssembler*>(this);
}

const MacroAssembler& MacroAssemblerLOONGARCH64::asMasm() const {
  return *static_cast<const MacroAssembler*>(this);
}

void MacroAssembler::subFromStackPtr(Imm32 imm32) {
  if (imm32.value) {
    asMasm().subPtr(imm32, StackPointer);
  }
}

//{{{ check_macroassembler_style
// ===============================================================
// MacroAssembler high-level usage.

void MacroAssembler::flush() {}

// ===============================================================
// Stack manipulation functions.

void MacroAssembler::PushRegsInMask(LiveRegisterSet set) {
  int32_t diff =
      set.gprs().size() * sizeof(intptr_t) + set.fpus().getPushSizeInBytes();
  const int32_t reserved = diff;

  reserveStack(reserved);
  for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ++iter) {
    diff -= sizeof(intptr_t);
    storePtr(*iter, Address(StackPointer, diff));
  }

#ifdef ENABLE_WASM_SIMD
#  error "Needs more careful logic if SIMD is enabled"
#endif

  for (FloatRegisterBackwardIterator iter(set.fpus().reduceSetForPush());
       iter.more(); ++iter) {
    diff -= sizeof(double);
    storeDouble(*iter, Address(StackPointer, diff));
  }
  MOZ_ASSERT(diff == 0);
}

void MacroAssembler::PopRegsInMaskIgnore(LiveRegisterSet set,
                                         LiveRegisterSet ignore) {
  int32_t diff =
      set.gprs().size() * sizeof(intptr_t) + set.fpus().getPushSizeInBytes();
  const int32_t reserved = diff;

  for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ++iter) {
    diff -= sizeof(intptr_t);
    if (!ignore.has(*iter)) {
      loadPtr(Address(StackPointer, diff), *iter);
    }
  }

#ifdef ENABLE_WASM_SIMD
#  error "Needs more careful logic if SIMD is enabled"
#endif

  for (FloatRegisterBackwardIterator iter(set.fpus().reduceSetForPush());
       iter.more(); ++iter) {
    diff -= sizeof(double);
    if (!ignore.has(*iter)) {
      loadDouble(Address(StackPointer, diff), *iter);
    }
  }
  MOZ_ASSERT(diff == 0);
  freeStack(reserved);
}

void MacroAssembler::Push(Register reg) {
  ma_push(reg);
  adjustFrame(int32_t(sizeof(intptr_t)));
}

void MacroAssembler::Push(const Imm32 imm) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, imm);
  ma_push(scratch);
  adjustFrame(int32_t(sizeof(intptr_t)));
}

void MacroAssembler::Push(const ImmWord imm) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, imm);
  ma_push(scratch);
  adjustFrame(int32_t(sizeof(intptr_t)));
}

void MacroAssembler::Push(const ImmPtr imm) {
  Push(ImmWord(uintptr_t(imm.value)));
}

void MacroAssembler::Push(const ImmGCPtr ptr) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, ptr);
  ma_push(scratch);
  adjustFrame(int32_t(sizeof(intptr_t)));
}

void MacroAssembler::Push(FloatRegister f) {
  ma_push(f);
  adjustFrame(int32_t(f.size()));
}

void MacroAssembler::Pop(Register reg) {
  ma_pop(reg);
  adjustFrame(-int32_t(sizeof(intptr_t)));
}

void MacroAssembler::Pop(FloatRegister f) {
  ma_pop(f);
  adjustFrame(-int32_t(f.size()));
}

void MacroAssembler::Pop(const ValueOperand& val) {
  popValue(val);
  adjustFrame(-int32_t(sizeof(Value)));
}

// ===============================================================
// Simple call functions.

CodeOffset MacroAssembler::call(Register reg) {
  as_jirl(ra, reg, BOffImm16(0));
  return CodeOffset(currentOffset());
}

CodeOffset MacroAssembler::call(Label* label) {
  ma_bl(label);
  return CodeOffset(currentOffset());
}

CodeOffset MacroAssembler::callWithPatch() {
  as_bl(JOffImm26(1 * sizeof(uint32_t)));
  return CodeOffset(currentOffset());
}

void MacroAssembler::patchCall(uint32_t callerOffset, uint32_t calleeOffset) {
  BufferOffset call(callerOffset - 1 * sizeof(uint32_t));

  JOffImm26 offset = BufferOffset(calleeOffset).diffB<JOffImm26>(call);
  if (!offset.isInvalid()) {
    InstJump* bal = (InstJump*)editSrc(call);
    bal->setJOffImm26(offset);
  } else {
    uint32_t u32Offset = callerOffset - 4 * sizeof(uint32_t);
    uint32_t* u32 =
        reinterpret_cast<uint32_t*>(editSrc(BufferOffset(u32Offset)));
    *u32 = calleeOffset - callerOffset;
  }
}

CodeOffset MacroAssembler::farJumpWithPatch() {
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  as_pcaddi(scratch, 4);
  as_ld_w(scratch2, scratch, 0);
  as_add_d(scratch, scratch, scratch2);
  as_jirl(zero, scratch, BOffImm16(0));
  // Allocate space which will be patched by patchFarJump().
  CodeOffset farJump(currentOffset());
  spew(".space 32bit initValue 0xffff ffff");
  writeInst(UINT32_MAX);
  return farJump;
}

void MacroAssembler::patchFarJump(CodeOffset farJump, uint32_t targetOffset) {
  uint32_t* u32 =
      reinterpret_cast<uint32_t*>(editSrc(BufferOffset(farJump.offset())));
  MOZ_ASSERT(*u32 == UINT32_MAX);
  *u32 = targetOffset - farJump.offset();
}

void MacroAssembler::repatchFarJump(uint8_t* code, uint32_t farJumpOffset,
                                    uint32_t targetOffset) {
  uint32_t* u32 = reinterpret_cast<uint32_t*>(code + farJumpOffset);
  *u32 = targetOffset - farJumpOffset;
}

CodeOffset MacroAssembler::nopPatchableToNearJump() {
  CodeOffset offset(currentOffset());
  as_nop();
  return offset;
}

void MacroAssembler::patchNopToNearJump(uint8_t* jump, uint8_t* target) {
  MOZ_ASSERT(reinterpret_cast<Instruction*>(jump)->is<InstNOP>());
  new (jump) InstJump(op_b, JOffImm26(target - jump));
}

void MacroAssembler::patchNearJumpToNop(uint8_t* jump) {
  MOZ_ASSERT(reinterpret_cast<Instruction*>(jump)->is<InstJump>());
  new (jump) InstNOP();
}

void MacroAssembler::call(wasm::SymbolicAddress target) {
  movePtr(target, CallReg);
  call(CallReg);
}

void MacroAssembler::call(ImmWord target) { call(ImmPtr((void*)target.value)); }

void MacroAssembler::call(ImmPtr target) {
  BufferOffset bo = m_buffer.nextOffset();
  addPendingJump(bo, target, Relocation::HARDCODED);
  ma_call(target);
}

void MacroAssembler::call(JitCode* c) {
  ScratchRegisterScope scratch(asMasm());
  BufferOffset bo = m_buffer.nextOffset();
  addPendingJump(bo, ImmPtr(c->raw()), Relocation::JITCODE);
  ma_liPatchable(scratch, ImmPtr(c->raw()));
  callJitNoProfiler(scratch);
}

CodeOffsetJump MacroAssemblerLOONGARCH64Compat::backedgeJump(
    RepatchLabel* label, Label* documentation) {
  return jumpWithPatch(label, documentation);
}

CodeOffsetJump MacroAssemblerLOONGARCH64Compat::jumpWithPatch(
    RepatchLabel* label, Label* documentation) {
  (void)documentation;
  MOZ_ASSERT(!label->used());

  BufferOffset bo = nextOffset();
  label->use(bo.getOffset());

  m_buffer.ensureSpace(4 * sizeof(uint32_t));
  if (label->bound()) {
    addLongJump(bo, BufferOffset(label->offset()));
  }
  ma_liPatchable(ScratchRegister, ImmWord(LabelBase::INVALID_OFFSET));
  as_jirl(zero, ScratchRegister, BOffImm16(0));
  return CodeOffsetJump(bo.getOffset());
}

void MacroAssembler::pushReturnAddress() { push(ra); }

void MacroAssembler::popReturnAddress() { pop(ra); }

// ===============================================================
// ABI function calls.

void MacroAssembler::setupUnalignedABICall(Register scratch) {
  MOZ_ASSERT(!IsCompilingWasm(), "wasm should only use aligned ABI calls");
  setupABICall();
  dynamicAlignment_ = true;

  as_or(scratch, StackPointer, zero);

  // Force sp to be aligned
  asMasm().subPtr(Imm32(sizeof(uintptr_t)), StackPointer);
  ma_and(StackPointer, StackPointer, Imm32(~(ABIStackAlignment - 1)));
  storePtr(scratch, Address(StackPointer, 0));
}

void MacroAssembler::callWithABIPre(uint32_t* stackAdjust, bool callFromWasm) {
  MOZ_ASSERT(inCall_);
  uint32_t stackForCall = abiArgs_.stackBytesConsumedSoFar();

  // Reserve place for $ra.
  stackForCall += sizeof(intptr_t);

  if (dynamicAlignment_) {
    stackForCall += ComputeByteAlignment(stackForCall, ABIStackAlignment);
  } else {
    uint32_t alignmentAtPrologue = callFromWasm ? sizeof(wasm::Frame) : 0;
    stackForCall += ComputeByteAlignment(
        stackForCall + framePushed() + alignmentAtPrologue, ABIStackAlignment);
  }

  *stackAdjust = stackForCall;
  reserveStack(stackForCall);

  // Save $ra because call is going to clobber it. Restore it in
  // callWithABIPost. NOTE: This is needed for calls from SharedIC.
  // Maybe we can do this differently.
  storePtr(ra, Address(StackPointer, stackForCall - sizeof(intptr_t)));

  // Position all arguments.
  {
    enoughMemory_ &= moveResolver_.resolve();
    if (!enoughMemory_) {
      return;
    }

    MoveEmitter emitter(*this);
    emitter.emit(moveResolver_);
    emitter.finish();
  }

  assertStackAlignment(ABIStackAlignment);
}

void MacroAssembler::callWithABIPost(uint32_t stackAdjust, MoveOp::Type result) {
  // Restore ra value (as stored in callWithABIPre()).
  loadPtr(Address(StackPointer, stackAdjust - sizeof(intptr_t)), ra);

  if (dynamicAlignment_) {
    // Restore sp value from stack (as stored in setupUnalignedABICall()).
    loadPtr(Address(StackPointer, stackAdjust), StackPointer);
    // Use adjustFrame instead of freeStack because we already restored sp.
    adjustFrame(-stackAdjust);
  } else {
    freeStack(stackAdjust);
  }

#ifdef DEBUG
  MOZ_ASSERT(inCall_);
  inCall_ = false;
#endif
}

void MacroAssembler::callWithABINoProfiler(Register fun, MoveOp::Type result) {
  SecondScratchRegisterScope scratch2(asMasm());
  // Load the callee in scratch2, no instruction between the movePtr and
  // call should clobber it. Note that we can't use fun because it may be
  // one of the IntArg registers clobbered before the call.
  movePtr(fun, scratch2);

  uint32_t stackAdjust;
  callWithABIPre(&stackAdjust);
  call(scratch2);
  callWithABIPost(stackAdjust, result);
}

void MacroAssembler::callWithABINoProfiler(const Address& fun,
                                           MoveOp::Type result) {
  SecondScratchRegisterScope scratch2(asMasm());
  // Load the callee in scratch2, as above.
  loadPtr(fun, scratch2);

  uint32_t stackAdjust;
  callWithABIPre(&stackAdjust);
  call(scratch2);
  callWithABIPost(stackAdjust, result);
}

// ===============================================================
// Jit Frames.

uint32_t MacroAssembler::pushFakeReturnAddress(Register scratch) {
  CodeLabel cl;

  ma_li(scratch, &cl);
  Push(scratch);
  bind(&cl);
  uint32_t retAddr = currentOffset();

  addCodeLabel(cl);
  return retAddr;
}

// ===============================================================
// Move instructions

void MacroAssemblerLOONGARCH64Compat::moveValue(const Value& src, Register dest) {
  if (!src.isGCThing()) {
    ma_li(dest, ImmWord(src.asRawBits()));
    return;
  }

  writeDataRelocation(src);
  movWithPatch(ImmWord(src.asRawBits()), dest);
}

void MacroAssemblerLOONGARCH64Compat::moveValue(const TypedOrValueRegister& src,
                               const ValueOperand& dest) {
  if (src.hasValue()) {
    moveValue(src.valueReg(), dest);
    return;
  }

  MIRType type = src.type();
  AnyRegister reg = src.typedReg();

  if (!IsFloatingPointType(type)) {
    boxNonDouble(ValueTypeFromMIRType(type), reg.gpr(), dest);
    return;
  }

  ScratchDoubleScope fpscratch(asMasm());
  FloatRegister scratch = fpscratch;
  FloatRegister freg = reg.fpu();
  if (type == MIRType::Float32) {
    convertFloat32ToDouble(freg, scratch);
    freg = scratch;
  }
  boxDouble(freg, dest, scratch);
}

void MacroAssemblerLOONGARCH64Compat::moveValue(const ValueOperand& src,
                               const ValueOperand& dest) {
  if (src == dest) {
    return;
  }
  movePtr(src.valueReg(), dest.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::moveValue(const Value& src, const ValueOperand& dest) {
  moveValue(src, dest.valueReg());
}

// ===============================================================
// Branch functions

void MacroAssembler::branchPtrInNurseryChunk(Condition cond, Register ptr,
                                             Register temp, Label* label) {
  MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
  MOZ_ASSERT(ptr != temp);
  MOZ_ASSERT(temp != SecondScratchReg);

  movePtr(ptr, temp);
  orPtr(Imm32(gc::ChunkMask), temp);
  load32(Address(temp, gc::ChunkLocationOffsetFromLastByte), temp);
  branch32(cond, temp, Imm32(int32_t(gc::ChunkLocation::Nursery)), label);
}

void MacroAssembler::branchValueIsNurseryObject(Condition cond,
                                                const Address& address,
                                                Register temp, Label* label) {
  MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);

  Label done;
  branchTestObject(Assembler::NotEqual, address,
                   cond == Assembler::Equal ? &done : label);

  extractObject(address, temp);
  orPtr(Imm32(gc::ChunkMask), temp);
  load32(Address(temp, gc::ChunkLocationOffsetFromLastByte), temp);
  branch32(cond, temp, Imm32(int32_t(gc::ChunkLocation::Nursery)), label);

  bind(&done);
}

void MacroAssembler::branchValueIsNurseryObject(Condition cond,
                                                ValueOperand value,
                                                Register temp, Label* label) {
  MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);

  Label done;
  branchTestObject(Assembler::NotEqual, value,
                   cond == Assembler::Equal ? &done : label);

  extractObject(value, temp);
  orPtr(Imm32(gc::ChunkMask), temp);
  load32(Address(temp, gc::ChunkLocationOffsetFromLastByte), temp);
  branch32(cond, temp, Imm32(int32_t(gc::ChunkLocation::Nursery)), label);

  bind(&done);
}

void MacroAssembler::branchTestValue(Condition cond, const ValueOperand& lhs,
                                     const Value& rhs, Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(lhs.valueReg() != scratch);
  moveValue(rhs, ValueOperand(scratch));
  ma_b(lhs.valueReg(), scratch, label, cond);
}

// ========================================================================
// Memory access primitives.

template <typename T>
void MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value,
                                       MIRType valueType, const T& dest,
                                       MIRType slotType) {
  if (valueType == MIRType::Double) {
    storeDouble(value.reg().typedReg().fpu(), dest);
    return;
  }

  // For known integers and booleans, we can just store the unboxed value if
  // the slot has the same type.
  if ((valueType == MIRType::Int32 || valueType == MIRType::Boolean) &&
      slotType == valueType) {
    if (value.constant()) {
      Value val = value.value();
      if (valueType == MIRType::Int32) {
        store32(Imm32(val.toInt32()), dest);
      } else {
        store32(Imm32(val.toBoolean() ? 1 : 0), dest);
      }
    } else {
      store32(value.reg().typedReg().gpr(), dest);
    }
    return;
  }

  if (value.constant()) {
    storeValue(value.value(), dest);
  } else {
    storeValue(ValueTypeFromMIRType(valueType), value.reg().typedReg().gpr(),
               dest);
  }
}

template void MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value,
                                                MIRType valueType,
                                                const Address& dest,
                                                MIRType slotType);
template void MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value,
                                                MIRType valueType,
                                                const BaseIndex& dest,
                                                MIRType slotType);
template void MacroAssembler::storeUnboxedValue(
    const ConstantOrRegister& value, MIRType valueType,
    const BaseObjectElementIndex& dest, MIRType slotType);

void MacroAssembler::comment(const char* msg) { Assembler::comment(msg); }

void MacroAssemblerLOONGARCH64Compat::move32(Imm32 imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerLOONGARCH64Compat::move32(Register src, Register dest) {
  as_slli_w(dest, src, 0);
}

void MacroAssemblerLOONGARCH64Compat::movePtr(Register src, Register dest) {
  as_or(dest, src, zero);
}
void MacroAssemblerLOONGARCH64Compat::movePtr(ImmWord imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerLOONGARCH64Compat::movePtr(ImmGCPtr imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerLOONGARCH64Compat::movePtr(ImmPtr imm, Register dest) {
  movePtr(ImmWord(uintptr_t(imm.value)), dest);
}

void MacroAssemblerLOONGARCH64Compat::movePtr(wasm::SymbolicAddress imm,
                                          Register dest) {
  append(wasm::SymbolicAccess(CodeOffset(nextOffset().getOffset()), imm));
  ma_liPatchable(dest, ImmWord(-1));
}

void MacroAssemblerLOONGARCH64Compat::load8ZeroExtend(const Address& address,
                                                  Register dest) {
  ma_load(dest, address, SizeByte, ZeroExtend);
}

void MacroAssemblerLOONGARCH64Compat::load8ZeroExtend(const BaseIndex& src,
                                                  Register dest) {
  ma_load(dest, src, SizeByte, ZeroExtend);
}

void MacroAssemblerLOONGARCH64Compat::load8SignExtend(const Address& address,
                                                  Register dest) {
  ma_load(dest, address, SizeByte, SignExtend);
}

void MacroAssemblerLOONGARCH64Compat::load8SignExtend(const BaseIndex& src,
                                                  Register dest) {
  ma_load(dest, src, SizeByte, SignExtend);
}

void MacroAssemblerLOONGARCH64Compat::load16ZeroExtend(const Address& address,
                                                   Register dest) {
  ma_load(dest, address, SizeHalfWord, ZeroExtend);
}

void MacroAssemblerLOONGARCH64Compat::load16ZeroExtend(const BaseIndex& src,
                                                   Register dest) {
  ma_load(dest, src, SizeHalfWord, ZeroExtend);
}

void MacroAssemblerLOONGARCH64Compat::load16SignExtend(const Address& address,
                                                   Register dest) {
  ma_load(dest, address, SizeHalfWord, SignExtend);
}

void MacroAssemblerLOONGARCH64Compat::load16SignExtend(const BaseIndex& src,
                                                   Register dest) {
  ma_load(dest, src, SizeHalfWord, SignExtend);
}

void MacroAssemblerLOONGARCH64Compat::load32(const Address& address,
                                         Register dest) {
  ma_ld_w(dest, address);
}

void MacroAssemblerLOONGARCH64Compat::load32(const BaseIndex& address,
                                         Register dest) {
  Register base = address.base;
  Register index = address.index;
  int32_t offset = address.offset;
  uint32_t shift = Imm32::ShiftOf(address.scale).value;

  if (offset != 0) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, Imm32(offset));
    if (shift != 0) {
      MOZ_ASSERT(shift <= 4);
      as_alsl_d(scratch, index, scratch, shift - 1);
    } else {
      as_add_d(scratch, index, scratch);
    }
    as_ldx_w(dest, base, scratch);
  } else if (shift != 0) {
    ScratchRegisterScope scratch(asMasm());
    as_slli_d(scratch, index, shift);
    as_ldx_w(dest, base, scratch);
  } else {
    as_ldx_w(dest, base, index);
  }
}

void MacroAssemblerLOONGARCH64Compat::load32(AbsoluteAddress address,
                                         Register dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(ImmPtr(address.addr), scratch);
  load32(Address(scratch, 0), dest);
}

void MacroAssemblerLOONGARCH64Compat::load32(wasm::SymbolicAddress address,
                                         Register dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(address, scratch);
  load32(Address(scratch, 0), dest);
}

void MacroAssemblerLOONGARCH64Compat::loadPtr(const Address& address,
                                          Register dest) {
  ma_ld_d(dest, address);
}

void MacroAssemblerLOONGARCH64Compat::loadPtr(const BaseIndex& src, Register dest) {
  Register base = src.base;
  Register index = src.index;
  int32_t offset = src.offset;
  uint32_t shift = Imm32::ShiftOf(src.scale).value;

  if (offset != 0) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, Imm32(offset));
    if (shift != 0) {
      MOZ_ASSERT(shift <= 4);
      as_alsl_d(scratch, index, scratch, shift - 1);
    } else {
      as_add_d(scratch, index, scratch);
    }
    as_ldx_d(dest, base, scratch);
  } else if (shift != 0) {
    ScratchRegisterScope scratch(asMasm());
    as_slli_d(scratch, index, shift);
    as_ldx_d(dest, base, scratch);
  } else {
    as_ldx_d(dest, base, index);
  }
}

void MacroAssemblerLOONGARCH64Compat::loadPtr(AbsoluteAddress address,
                                          Register dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(ImmPtr(address.addr), scratch);
  loadPtr(Address(scratch, 0), dest);
}

void MacroAssemblerLOONGARCH64Compat::loadPtr(wasm::SymbolicAddress address,
                                          Register dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(address, scratch);
  loadPtr(Address(scratch, 0), dest);
}

void MacroAssemblerLOONGARCH64Compat::loadPrivate(const Address& address,
                                              Register dest) {
  loadPtr(address, dest);
  as_slli_d(dest, dest, 1);
}

void MacroAssemblerLOONGARCH64Compat::store8(Imm32 imm, const Address& address) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch2, imm);
  ma_store(scratch2, address, SizeByte);
}

void MacroAssemblerLOONGARCH64Compat::store8(Register src, const Address& address) {
  ma_store(src, address, SizeByte);
}

void MacroAssemblerLOONGARCH64Compat::store8(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeByte);
}

void MacroAssemblerLOONGARCH64Compat::store8(Register src, const BaseIndex& dest) {
  ma_store(src, dest, SizeByte);
}

void MacroAssemblerLOONGARCH64Compat::store16(Imm32 imm, const Address& address) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch2, imm);
  ma_store(scratch2, address, SizeHalfWord);
}

void MacroAssemblerLOONGARCH64Compat::store16(Register src,
                                          const Address& address) {
  ma_store(src, address, SizeHalfWord);
}

void MacroAssemblerLOONGARCH64Compat::store16(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeHalfWord);
}

void MacroAssemblerLOONGARCH64Compat::store16(Register src,
                                          const BaseIndex& address) {
  ma_store(src, address, SizeHalfWord);
}

void MacroAssemblerLOONGARCH64Compat::store32(Register src,
                                          AbsoluteAddress address) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(ImmPtr(address.addr), scratch);
  store32(src, Address(scratch, 0));
}

void MacroAssemblerLOONGARCH64Compat::store32(Register src,
                                          const Address& address) {
  ma_store(src, address, SizeWord);
}

void MacroAssemblerLOONGARCH64Compat::store32(Imm32 src, const Address& address) {
  SecondScratchRegisterScope scratch2(asMasm());
  move32(src, scratch2);
  ma_store(scratch2, address, SizeWord);
}

void MacroAssemblerLOONGARCH64Compat::store32(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeWord);
}

void MacroAssemblerLOONGARCH64Compat::store32(Register src, const BaseIndex& dest) {
  ma_store(src, dest, SizeWord);
}

template <typename T>
void MacroAssemblerLOONGARCH64Compat::storePtr(ImmWord imm, T address) {
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch2, imm);
  ma_store(scratch2, address, SizeDouble);
}

template void MacroAssemblerLOONGARCH64Compat::storePtr<Address>(ImmWord imm,
                                                             Address address);
template void MacroAssemblerLOONGARCH64Compat::storePtr<BaseIndex>(
    ImmWord imm, BaseIndex address);

template <typename T>
void MacroAssemblerLOONGARCH64Compat::storePtr(ImmPtr imm, T address) {
  storePtr(ImmWord(uintptr_t(imm.value)), address);
}

template void MacroAssemblerLOONGARCH64Compat::storePtr<Address>(ImmPtr imm,
                                                             Address address);
template void MacroAssemblerLOONGARCH64Compat::storePtr<BaseIndex>(
    ImmPtr imm, BaseIndex address);

template <typename T>
void MacroAssemblerLOONGARCH64Compat::storePtr(ImmGCPtr imm, T address) {
  SecondScratchRegisterScope scratch2(asMasm());
  movePtr(imm, scratch2);
  storePtr(scratch2, address);
}

template void MacroAssemblerLOONGARCH64Compat::storePtr<Address>(ImmGCPtr imm,
                                                             Address address);
template void MacroAssemblerLOONGARCH64Compat::storePtr<BaseIndex>(
    ImmGCPtr imm, BaseIndex address);

void MacroAssemblerLOONGARCH64Compat::storePtr(Register src,
                                           const Address& address) {
  ma_st_d(src, address);
}

void MacroAssemblerLOONGARCH64Compat::storePtr(Register src,
                                           const BaseIndex& address) {
  Register base = address.base;
  Register index = address.index;
  int32_t offset = address.offset;
  int32_t shift = Imm32::ShiftOf(address.scale).value;

  if ((offset == 0) && (shift == 0)) {
    as_stx_d(src, base, index);
  } else if (is_intN(offset, 12)) {
    ScratchRegisterScope scratch(asMasm());
    if (shift == 0) {
      as_add_d(scratch, base, index);
    } else {
      as_alsl_d(scratch, index, base, shift - 1);
    }
    as_st_d(src, scratch, offset);
  } else {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, Imm32(offset));
    if (shift == 0) {
      as_add_d(scratch, scratch, index);
    } else {
      as_alsl_d(scratch, index, scratch, shift - 1);
    }
    as_stx_d(src, base, scratch);
  }
}

void MacroAssemblerLOONGARCH64Compat::storePtr(Register src, AbsoluteAddress dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(ImmPtr(dest.addr), scratch);
  storePtr(src, Address(scratch, 0));
}

void MacroAssemblerLOONGARCH64Compat::atomicEffectOp(
    int nbytes, AtomicOp op, Imm32 value, const Address& address,
    Register valueTemp, Register offsetTemp, Register maskTemp) {
  AtomicFetchOpLoongArch64(*this, nbytes, false, op, value, address, valueTemp,
                           offsetTemp, maskTemp, InvalidReg);
}

void MacroAssemblerLOONGARCH64Compat::atomicEffectOp(
    int nbytes, AtomicOp op, Imm32 value, const BaseIndex& address,
    Register valueTemp, Register offsetTemp, Register maskTemp) {
  AtomicFetchOpLoongArch64(*this, nbytes, false, op, value, address, valueTemp,
                           offsetTemp, maskTemp, InvalidReg);
}

void MacroAssemblerLOONGARCH64Compat::atomicEffectOp(
    int nbytes, AtomicOp op, Register value, const Address& address,
    Register valueTemp, Register offsetTemp, Register maskTemp) {
  AtomicFetchOpLoongArch64(*this, nbytes, false, op, value, address, valueTemp,
                           offsetTemp, maskTemp, InvalidReg);
}

void MacroAssemblerLOONGARCH64Compat::atomicEffectOp(
    int nbytes, AtomicOp op, Register value, const BaseIndex& address,
    Register valueTemp, Register offsetTemp, Register maskTemp) {
  AtomicFetchOpLoongArch64(*this, nbytes, false, op, value, address, valueTemp,
                           offsetTemp, maskTemp, InvalidReg);
}

void MacroAssemblerLOONGARCH64Compat::atomicFetchOp(
    int nbytes, bool signExtend, AtomicOp op, Imm32 value,
    const Address& address, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  AtomicFetchOpLoongArch64(*this, nbytes, signExtend, op, value, address,
                           valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::atomicFetchOp(
    int nbytes, bool signExtend, AtomicOp op, Imm32 value,
    const BaseIndex& address, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  AtomicFetchOpLoongArch64(*this, nbytes, signExtend, op, value, address,
                           valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::atomicFetchOp(
    int nbytes, bool signExtend, AtomicOp op, Register value,
    const Address& address, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  AtomicFetchOpLoongArch64(*this, nbytes, signExtend, op, value, address,
                           valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::atomicFetchOp(
    int nbytes, bool signExtend, AtomicOp op, Register value,
    const BaseIndex& address, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  AtomicFetchOpLoongArch64(*this, nbytes, signExtend, op, value, address,
                           valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::compareExchange(
    int nbytes, bool signExtend, const Address& address, Register oldval,
    Register newval, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  CompareExchangeLoongArch64(*this, nbytes, signExtend, address, oldval,
                             newval, valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::compareExchange(
    int nbytes, bool signExtend, const BaseIndex& address, Register oldval,
    Register newval, Register valueTemp, Register offsetTemp,
    Register maskTemp, Register output) {
  CompareExchangeLoongArch64(*this, nbytes, signExtend, address, oldval,
                             newval, valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::compareExchangeToTypedIntArray(
    Scalar::Type arrayType, const Address& mem, Register oldval,
    Register newval, Register temp, Register valueTemp, Register offsetTemp,
    Register maskTemp, AnyRegister output) {
  switch (arrayType) {
    case Scalar::Int8:
      compareExchange(1, true, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint8:
      compareExchange(1, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Int16:
      compareExchange(2, true, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint16:
      compareExchange(2, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Int32:
      compareExchange(4, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint32:
      MOZ_ASSERT(output.isFloat());
      MOZ_ASSERT(temp != InvalidReg);
      compareExchange(4, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, temp);
      convertUInt32ToDouble(temp, output.fpu());
      break;
    default:
      MOZ_CRASH("Invalid typed array type");
  }
}

void MacroAssemblerLOONGARCH64Compat::compareExchangeToTypedIntArray(
    Scalar::Type arrayType, const BaseIndex& mem, Register oldval,
    Register newval, Register temp, Register valueTemp, Register offsetTemp,
    Register maskTemp, AnyRegister output) {
  switch (arrayType) {
    case Scalar::Int8:
      compareExchange(1, true, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint8:
      compareExchange(1, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Int16:
      compareExchange(2, true, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint16:
      compareExchange(2, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Int32:
      compareExchange(4, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, output.gpr());
      break;
    case Scalar::Uint32:
      MOZ_ASSERT(output.isFloat());
      MOZ_ASSERT(temp != InvalidReg);
      compareExchange(4, false, mem, oldval, newval, valueTemp, offsetTemp,
                      maskTemp, temp);
      convertUInt32ToDouble(temp, output.fpu());
      break;
    default:
      MOZ_CRASH("Invalid typed array type");
  }
}

void MacroAssemblerLOONGARCH64Compat::atomicExchange(
    int nbytes, bool signExtend, const Address& address, Register value,
    Register valueTemp, Register offsetTemp, Register maskTemp,
    Register output) {
  CompareExchangeLoongArch64(*this, nbytes, signExtend, address, InvalidReg,
                             value, valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::atomicExchange(
    int nbytes, bool signExtend, const BaseIndex& address, Register value,
    Register valueTemp, Register offsetTemp, Register maskTemp,
    Register output) {
  CompareExchangeLoongArch64(*this, nbytes, signExtend, address, InvalidReg,
                             value, valueTemp, offsetTemp, maskTemp, output);
}

void MacroAssemblerLOONGARCH64Compat::atomicExchangeToTypedIntArray(
    Scalar::Type arrayType, const Address& mem, Register value, Register temp,
    Register valueTemp, Register offsetTemp, Register maskTemp,
    AnyRegister output) {
  switch (arrayType) {
    case Scalar::Int8:
      atomicExchange(1, true, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint8:
      atomicExchange(1, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Int16:
      atomicExchange(2, true, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint16:
      atomicExchange(2, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Int32:
      atomicExchange(4, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint32:
      MOZ_ASSERT(output.isFloat());
      MOZ_ASSERT(temp != InvalidReg);
      atomicExchange(4, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     temp);
      convertUInt32ToDouble(temp, output.fpu());
      break;
    default:
      MOZ_CRASH("Invalid typed array type");
  }
}

void MacroAssemblerLOONGARCH64Compat::atomicExchangeToTypedIntArray(
    Scalar::Type arrayType, const BaseIndex& mem, Register value,
    Register temp, Register valueTemp, Register offsetTemp,
    Register maskTemp, AnyRegister output) {
  switch (arrayType) {
    case Scalar::Int8:
      atomicExchange(1, true, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint8:
      atomicExchange(1, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Int16:
      atomicExchange(2, true, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint16:
      atomicExchange(2, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Int32:
      atomicExchange(4, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     output.gpr());
      break;
    case Scalar::Uint32:
      MOZ_ASSERT(output.isFloat());
      MOZ_ASSERT(temp != InvalidReg);
      atomicExchange(4, false, mem, value, valueTemp, offsetTemp, maskTemp,
                     temp);
      convertUInt32ToDouble(temp, output.fpu());
      break;
    default:
      MOZ_CRASH("Invalid typed array type");
  }
}

void MacroAssemblerLOONGARCH64Compat::testNullSet(Condition cond,
                                              const ValueOperand& value,
                                              Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  SecondScratchRegisterScope scratch2(asMasm());
  splitTag(value, scratch2);
  ma_cmp_set(dest, scratch2, ImmTag(JSVAL_TAG_NULL), cond);
}

void MacroAssemblerLOONGARCH64Compat::testObjectSet(Condition cond,
                                                const ValueOperand& value,
                                                Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  SecondScratchRegisterScope scratch2(asMasm());
  splitTag(value, scratch2);
  ma_cmp_set(dest, scratch2, ImmTag(JSVAL_TAG_OBJECT), cond);
}

void MacroAssemblerLOONGARCH64Compat::testUndefinedSet(Condition cond,
                                                   const ValueOperand& value,
                                                   Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  SecondScratchRegisterScope scratch2(asMasm());
  splitTag(value, scratch2);
  ma_cmp_set(dest, scratch2, ImmTag(JSVAL_TAG_UNDEFINED), cond);
}

void MacroAssemblerLOONGARCH64Compat::unboxInt32(const ValueOperand& operand,
                                             Register dest) {
  as_slli_w(dest, operand.valueReg(), 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxInt32(Register src, Register dest) {
  as_slli_w(dest, src, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxInt32(const Address& src,
                                             Register dest) {
  load32(Address(src.base, src.offset), dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxInt32(const BaseIndex& src,
                                             Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  computeScaledAddress(src, scratch2);
  load32(Address(scratch2, src.offset), dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxBoolean(const ValueOperand& operand,
                                               Register dest) {
  as_slli_w(dest, operand.valueReg(), 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxBoolean(Register src, Register dest) {
  as_slli_w(dest, src, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxBoolean(const Address& src,
                                               Register dest) {
  ma_ld_w(dest, src);
}

void MacroAssemblerLOONGARCH64Compat::unboxBoolean(const BaseIndex& src,
                                               Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  computeScaledAddress(src, scratch2);
  ma_ld_w(dest, Address(scratch2, src.offset));
}

void MacroAssemblerLOONGARCH64Compat::unboxDouble(const ValueOperand& operand,
                                              FloatRegister dest) {
  as_movgr2fr_d(dest, operand.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::unboxDouble(const Address& src,
                                              FloatRegister dest) {
  ma_fld_d(dest, Address(src.base, src.offset));
}

void MacroAssemblerLOONGARCH64Compat::unboxDouble(const BaseIndex& src,
                                              FloatRegister dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(src, scratch2);
  unboxDouble(ValueOperand(scratch2), dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxString(const ValueOperand& operand,
                                              Register dest) {
  unboxNonDouble(operand, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxString(Register src, Register dest) {
  as_bstrpick_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxString(const Address& src,
                                              Register dest) {
  unboxNonDouble(src, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxSymbol(const ValueOperand& operand,
                                              Register dest) {
  unboxNonDouble(operand, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxSymbol(Register src, Register dest) {
  as_bstrpick_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxSymbol(const Address& src,
                                              Register dest) {
  unboxNonDouble(src, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxBigInt(const ValueOperand& operand,
                                              Register dest) {
  unboxNonDouble(operand, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxBigInt(Register src, Register dest) {
  as_bstrpick_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxBigInt(const Address& src,
                                              Register dest) {
  unboxNonDouble(src, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxObject(const ValueOperand& src,
                                              Register dest) {
  unboxNonDouble(src, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxObject(Register src, Register dest) {
  as_bstrpick_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
}

void MacroAssemblerLOONGARCH64Compat::unboxObject(const Address& src,
                                              Register dest) {
  unboxNonDouble(src, dest);
}

void MacroAssemblerLOONGARCH64Compat::unboxNonDouble(const ValueOperand& operand,
                                                 Register dest) {
  Label isInt32, done;
  SecondScratchRegisterScope scratch2(asMasm());
  splitTag(operand, scratch2);
  asMasm().branchTestInt32(Assembler::Equal, scratch2, &isInt32);

  as_bstrpick_d(dest, operand.valueReg(), JSVAL_TAG_SHIFT - 1, 0);
  jump(&done);

  bind(&isInt32);
  as_slli_w(dest, operand.valueReg(), 0);

  bind(&done);
}

void MacroAssemblerLOONGARCH64Compat::unboxNonDouble(const Address& src,
                                                 Register dest) {
  Label isInt32, done;
  loadPtr(src, dest);
  SecondScratchRegisterScope scratch2(asMasm());
  splitTag(dest, scratch2);
  asMasm().branchTestInt32(Assembler::Equal, scratch2, &isInt32);

  as_bstrpick_d(dest, dest, JSVAL_TAG_SHIFT - 1, 0);
  jump(&done);

  bind(&isInt32);
  as_slli_w(dest, dest, 0);

  bind(&done);
}

void MacroAssemblerLOONGARCH64Compat::unboxNonDouble(const BaseIndex& src,
                                                 Register dest) {
  Label isInt32, done;
  computeScaledAddress(src, SecondScratchReg);
  loadPtr(Address(SecondScratchReg, src.offset), dest);
  splitTag(dest, SecondScratchReg);
  asMasm().branchTestInt32(Assembler::Equal, SecondScratchReg, &isInt32);

  as_bstrpick_d(dest, dest, JSVAL_TAG_SHIFT - 1, 0);
  jump(&done);

  bind(&isInt32);
  as_slli_w(dest, dest, 0);

  bind(&done);
}

void MacroAssemblerLOONGARCH64Compat::unboxValue(const ValueOperand& src,
                                             AnyRegister dest) {
  if (dest.isFloat()) {
    Label notInt32, end;
    asMasm().branchTestInt32(Assembler::NotEqual, src, &notInt32);
    convertInt32ToDouble(src.valueReg(), dest.fpu());
    ma_b(&end, ShortJump);
    bind(&notInt32);
    unboxDouble(src, dest.fpu());
    bind(&end);
  } else {
    unboxNonDouble(src, dest.gpr());
  }
}

void MacroAssemblerLOONGARCH64Compat::boxDouble(FloatRegister src,
                                            const ValueOperand& dest,
                                            FloatRegister) {
  as_movfr2gr_d(dest.valueReg(), src);
}

void MacroAssemblerLOONGARCH64Compat::boxNonDouble(JSValueType type, Register src,
                                               const ValueOperand& dest) {
  boxValue(type, src, dest.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::boolValueToDouble(const ValueOperand& operand,
                                                    FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  convertBoolToInt32(operand.valueReg(), scratch);
  convertInt32ToDouble(scratch, dest);
}

void MacroAssemblerLOONGARCH64Compat::int32ValueToDouble(
    const ValueOperand& operand, FloatRegister dest) {
  convertInt32ToDouble(operand.valueReg(), dest);
}

void MacroAssemblerLOONGARCH64Compat::boolValueToFloat32(
    const ValueOperand& operand, FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  convertBoolToInt32(operand.valueReg(), scratch);
  convertInt32ToFloat32(scratch, dest);
}

void MacroAssemblerLOONGARCH64Compat::int32ValueToFloat32(
    const ValueOperand& operand, FloatRegister dest) {
  convertInt32ToFloat32(operand.valueReg(), dest);
}

void MacroAssemblerLOONGARCH64Compat::loadConstantFloat32(float f,
                                                      FloatRegister dest) {
  ma_lis(dest, f);
}

void MacroAssemblerLOONGARCH64Compat::loadInt32OrDouble(const Address& src,
                                                    FloatRegister dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  Label end;

  // If it's an int, convert it to double.
  loadPtr(Address(src.base, src.offset), scratch2);
  as_movgr2fr_d(dest, scratch2);
  as_srli_d(scratch2, scratch2, JSVAL_TAG_SHIFT);
  asMasm().branchTestInt32(Assembler::NotEqual, scratch2, &end);
  as_ffint_d_w(dest, dest);

  bind(&end);
}

void MacroAssemblerLOONGARCH64Compat::loadInt32OrDouble(const BaseIndex& addr,
                                                    FloatRegister dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  Label end;

  // If it's an int, convert it to double.
  computeScaledAddress(addr, scratch2);
  // Since we only have one scratch, we need to stomp over it with the tag.
  loadPtr(Address(scratch2, 0), scratch2);
  as_movgr2fr_d(dest, scratch2);
  as_srli_d(scratch2, scratch2, JSVAL_TAG_SHIFT);
  asMasm().branchTestInt32(Assembler::NotEqual, scratch2, &end);
  as_ffint_d_w(dest, dest);

  bind(&end);
}

void MacroAssemblerLOONGARCH64Compat::loadConstantDouble(double dp,
                                                     FloatRegister dest) {
  ma_lid(dest, dp);
}

Register MacroAssemblerLOONGARCH64Compat::extractObject(const Address& address,
                                                    Register scratch) {
  loadPtr(Address(address.base, address.offset), scratch);
  as_bstrpick_d(scratch, scratch, JSVAL_TAG_SHIFT - 1, 0);
  return scratch;
}

Register MacroAssemblerLOONGARCH64Compat::extractTag(const Address& address,
                                                 Register scratch) {
  loadPtr(Address(address.base, address.offset), scratch);
  as_bstrpick_d(scratch, scratch, 63, JSVAL_TAG_SHIFT);
  return scratch;
}

Register MacroAssemblerLOONGARCH64Compat::extractTag(const BaseIndex& address,
                                                 Register scratch) {
  computeScaledAddress(address, scratch);
  return extractTag(Address(scratch, address.offset), scratch);
}

/////////////////////////////////////////////////////////////////
// X86/X64-common/ARM/LoongArch interface.
/////////////////////////////////////////////////////////////////
void MacroAssemblerLOONGARCH64Compat::storeValue(ValueOperand val,
                                             const Address& dest) {
  storePtr(val.valueReg(), Address(dest.base, dest.offset));
}

void MacroAssemblerLOONGARCH64Compat::storeValue(ValueOperand val,
                                             const BaseIndex& dest) {
  storePtr(val.valueReg(), dest);
}

void MacroAssemblerLOONGARCH64Compat::storeValue(JSValueType type, Register reg,
                                             Address dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(dest.base != scratch2);

  tagValue(type, reg, ValueOperand(scratch2));
  storePtr(scratch2, dest);
}

void MacroAssemblerLOONGARCH64Compat::storeValue(JSValueType type, Register reg,
                                             BaseIndex dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(dest.base != scratch2);

  tagValue(type, reg, ValueOperand(scratch2));
  storePtr(scratch2, dest);
}

void MacroAssemblerLOONGARCH64Compat::storeValue(const Value& val, Address dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(dest.base != scratch2);

  if (val.isGCThing()) {
    writeDataRelocation(val);
    movWithPatch(ImmWord(val.asRawBits()), scratch2);
  } else {
    ma_li(scratch2, ImmWord(val.asRawBits()));
  }
  storePtr(scratch2, dest);
}

void MacroAssemblerLOONGARCH64Compat::storeValue(const Value& val, BaseIndex dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(dest.base != scratch2);

  if (val.isGCThing()) {
    writeDataRelocation(val);
    movWithPatch(ImmWord(val.asRawBits()), scratch2);
  } else {
    ma_li(scratch2, ImmWord(val.asRawBits()));
  }
  storePtr(scratch2, dest);
}

void MacroAssemblerLOONGARCH64Compat::loadValue(Address src, ValueOperand val) {
  loadPtr(src, val.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::loadValue(const BaseIndex& src,
                                            ValueOperand val) {
  loadPtr(src, val.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::tagValue(JSValueType type, Register payload,
                                           ValueOperand dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(dest.valueReg() != scratch);

  if (payload == dest.valueReg()) {
    as_or(scratch, payload, zero);
    payload = scratch;
  }
  ma_li(dest.valueReg(), Imm32(int32_t(JSVAL_TYPE_TO_TAG(type))));
  as_slli_d(dest.valueReg(), dest.valueReg(), JSVAL_TAG_SHIFT);
  if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
    as_bstrins_d(dest.valueReg(), payload, 31, 0);
  } else {
    as_bstrins_d(dest.valueReg(), payload, JSVAL_TAG_SHIFT - 1, 0);
  }
}

void MacroAssemblerLOONGARCH64Compat::pushValue(ValueOperand val) {
  push(val.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::pushValue(const Address& addr) { push(addr); }

void MacroAssemblerLOONGARCH64Compat::popValue(ValueOperand val) {
  pop(val.valueReg());
}

void MacroAssemblerLOONGARCH64Compat::breakpoint(uint32_t value) {
  as_break(value);
}

void MacroAssemblerLOONGARCH64Compat::handleFailureWithHandlerTail(void* handler) {
  // Reserve space for exception information.
  int size = (sizeof(ResumeFromException) + ABIStackAlignment) &
             ~(ABIStackAlignment - 1);
  asMasm().subPtr(Imm32(size), StackPointer);
  mov(StackPointer, a0);  // Use a0 since it is the first function argument.

  // Call the handler.
  asMasm().setupUnalignedABICall(a1);
  asMasm().passABIArg(a0);
  asMasm().callWithABI(handler);

  Label entryFrame;
  Label catch_;
  Label finally;
  Label return_;
  Label bailout;

  // Already clobbered a0, so use it...
  load32(Address(StackPointer, offsetof(ResumeFromException, kind)), a0);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ResumeFromException::RESUME_ENTRY_FRAME),
                    &entryFrame);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ResumeFromException::RESUME_CATCH), &catch_);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ResumeFromException::RESUME_FINALLY), &finally);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ResumeFromException::RESUME_FORCED_RETURN),
                    &return_);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ResumeFromException::RESUME_BAILOUT), &bailout);

  breakpoint();  // Invalid kind.

  // No exception handler. Load the error value, load the new stack pointer
  // and return from the entry frame.
  bind(&entryFrame);
  asMasm().moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)),
          StackPointer);
  ret();

  // If we found a catch handler, this must be a baseline frame. Restore
  // state and jump to the catch block.
  bind(&catch_);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, target)), a0);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, framePointer)),
          BaselineFrameReg);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)),
          StackPointer);
  jump(a0);

  // If we found a finally block, this must be a baseline frame. Push
  // two values expected by JSOP_RETSUB: BooleanValue(true) and the
  // exception.
  bind(&finally);
  ValueOperand exception = ValueOperand(a1);
  loadValue(Address(sp, offsetof(ResumeFromException, exception)), exception);

  loadPtr(Address(sp, offsetof(ResumeFromException, target)), a0);
  loadPtr(Address(sp, offsetof(ResumeFromException, framePointer)),
          BaselineFrameReg);
  loadPtr(Address(sp, offsetof(ResumeFromException, stackPointer)), sp);

  pushValue(BooleanValue(true));
  pushValue(exception);
  jump(a0);

  // Only used in debug mode. Return BaselineFrame->returnValue() to the
  // caller.
  bind(&return_);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, framePointer)),
          BaselineFrameReg);
  loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)),
          StackPointer);
  loadValue(
      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfReturnValue()),
      JSReturnOperand);
  movePtr(BaselineFrameReg, StackPointer);
  pop(BaselineFrameReg);

  // If profiling is enabled, then update the lastProfilingFrame to refer to
  // caller frame before returning.
  {
    Label skipProfilingInstrumentation;
    AbsoluteAddress addressOfEnabled(
        GetJitContext()->runtime->spsProfiler().addressOfEnabled());
    asMasm().branch32(Assembler::Equal, addressOfEnabled, Imm32(0),
                      &skipProfilingInstrumentation);
    profilerExitFrame();
    bind(&skipProfilingInstrumentation);
  }

  ret();

  // If we are bailing out to baseline to handle an exception, jump to
  // the bailout tail stub.
  bind(&bailout);
  loadPtr(Address(sp, offsetof(ResumeFromException, bailoutInfo)), a2);
  ma_li(ReturnReg, Imm32(BAILOUT_RETURN_OK));
  loadPtr(Address(sp, offsetof(ResumeFromException, target)), a1);
  jump(a1);
}

CodeOffset MacroAssemblerLOONGARCH64Compat::toggledJump(Label* label) {
  CodeOffset ret(nextOffset().getOffset());
  ma_b(label);
  return ret;
}

CodeOffset MacroAssemblerLOONGARCH64Compat::toggledCall(JitCode* target,
                                                    bool enabled) {
  ScratchRegisterScope scratch(asMasm());
  BufferOffset bo = nextOffset();
  CodeOffset offset(bo.getOffset());  // first instruction location,not changed.
  addPendingJump(bo, ImmPtr(target->raw()), Relocation::JITCODE);
  ma_liPatchable(scratch, ImmPtr(target->raw()));
  if (enabled) {
    as_jirl(ra, scratch, BOffImm16(0));
  } else {
    as_nop();
  }
  MOZ_ASSERT_IF(!oom(), nextOffset().getOffset() - offset.offset() ==
                            ToggledCallSize(nullptr));
  return offset;  // location of first instruction of call instr sequence.
}

//}}} check_macroassembler_style

}  // namespace jit
}  // namespace js
