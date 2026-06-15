/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_loongarch64_MacroAssembler_loongarch64_inl_h
#define jit_loongarch64_MacroAssembler_loongarch64_inl_h

#include "jit/loongarch64/MacroAssembler-loongarch64.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style

void MacroAssembler::move64(Register64 src, Register64 dest) {
  movePtr(src.reg, dest.reg);
}

void MacroAssembler::move64(Imm64 imm, Register64 dest) {
  movePtr(ImmWord(imm.value), dest.reg);
}
void MacroAssembler::moveFloat32ToGPR(FloatRegister src, Register dest) {
  moveFromFloat32(src, dest);
}

void MacroAssembler::moveGPRToFloat32(Register src, FloatRegister dest) {
  moveToFloat32(src, dest);
}

void MacroAssembler::move8SignExtend(Register src, Register dest) {
  as_ext_w_b(dest, src);
}

void MacroAssembler::move16SignExtend(Register src, Register dest) {
  as_ext_w_h(dest, src);
}
// ===============================================================
// Load instructions
// ===============================================================
// Logical instructions

void MacroAssembler::not32(Register reg) { as_nor(reg, reg, zero); }
void MacroAssembler::andPtr(Register src, Register dest) {
  as_and(dest, dest, src);
}

void MacroAssembler::andPtr(Imm32 imm, Register dest) {
  ma_and(dest, dest, imm);
}

void MacroAssembler::and64(Imm64 imm, Register64 dest) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, ImmWord(imm.value));
  as_and(dest.reg, dest.reg, scratch);
}

void MacroAssembler::and64(Register64 src, Register64 dest) {
  as_and(dest.reg, dest.reg, src.reg);
}

void MacroAssembler::and64(const Operand& src, Register64 dest) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(*this);
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    and64(scratch64, dest);
  } else {
    and64(Register64(src.toReg()), dest);
  }
}

void MacroAssembler::and32(Register src, Register dest) {
  as_and(dest, dest, src);
}

void MacroAssembler::and32(Imm32 imm, Register dest) {
  ma_and(dest, dest, imm);
}

void MacroAssembler::and32(Imm32 imm, const Address& dest) {
  if (dest.base == SecondScratchReg) {
    ScratchRegisterScope scratch(asMasm());
    load32(dest, scratch);
    and32(imm, scratch);
    store32(scratch, dest);
    return;
  }

  SecondScratchRegisterScope scratch2(asMasm());
  load32(dest, scratch2);
  and32(imm, scratch2);
  store32(scratch2, dest);
}

void MacroAssembler::and32(const Address& src, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(src, scratch2);
  as_and(dest, dest, scratch2);
}

void MacroAssembler::or64(Imm64 imm, Register64 dest) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, ImmWord(imm.value));
  as_or(dest.reg, dest.reg, scratch);
}

void MacroAssembler::or32(Register src, Register dest) {
  as_or(dest, dest, src);
}

void MacroAssembler::or32(Imm32 imm, Register dest) { ma_or(dest, dest, imm); }

void MacroAssembler::or32(Imm32 imm, const Address& dest) {
  if (dest.base == SecondScratchReg) {
    ScratchRegisterScope scratch(asMasm());
    load32(dest, scratch);
    or32(imm, scratch);
    store32(scratch, dest);
    return;
  }

  SecondScratchRegisterScope scratch2(asMasm());
  load32(dest, scratch2);
  or32(imm, scratch2);
  store32(scratch2, dest);
}

void MacroAssembler::xor64(Imm64 imm, Register64 dest) {
  ScratchRegisterScope scratch(asMasm());
  ma_li(scratch, ImmWord(imm.value));
  as_xor(dest.reg, dest.reg, scratch);
}

void MacroAssembler::orPtr(Register src, Register dest) {
  as_or(dest, dest, src);
}

void MacroAssembler::orPtr(Imm32 imm, Register dest) { ma_or(dest, dest, imm); }

void MacroAssembler::or64(Register64 src, Register64 dest) {
  as_or(dest.reg, dest.reg, src.reg);
}

void MacroAssembler::or64(const Operand& src, Register64 dest) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(asMasm());
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    or64(scratch64, dest);
  } else {
    or64(Register64(src.toReg()), dest);
  }
}

void MacroAssembler::xor64(Register64 src, Register64 dest) {
  as_xor(dest.reg, dest.reg, src.reg);
}

void MacroAssembler::xor64(const Operand& src, Register64 dest) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(asMasm());
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    xor64(scratch64, dest);
  } else {
    xor64(Register64(src.toReg()), dest);
  }
}

void MacroAssembler::xorPtr(Register src, Register dest) {
  as_xor(dest, dest, src);
}

void MacroAssembler::xorPtr(Imm32 imm, Register dest) {
  ma_xor(dest, dest, imm);
}

void MacroAssembler::xor32(Register src, Register dest) {
  as_xor(dest, dest, src);
}

void MacroAssembler::xor32(Imm32 imm, Register dest) {
  ma_xor(dest, dest, imm);
}

// ===============================================================
// Swap instructions
// ===============================================================
// Arithmetic functions

void MacroAssembler::addPtr(Register src, Register dest) {
  as_add_d(dest, dest, src);
}

void MacroAssembler::addPtr(Imm32 imm, Register dest) {
  ma_add_d(dest, dest, imm);
}

void MacroAssembler::addPtr(ImmWord imm, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  movePtr(imm, scratch);
  addPtr(scratch, dest);
}

void MacroAssembler::add64(Register64 src, Register64 dest) {
  addPtr(src.reg, dest.reg);
}

void MacroAssembler::add64(const Operand& src, Register64 dest) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(asMasm());
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    add64(scratch64, dest);
  } else {
    add64(Register64(src.toReg()), dest);
  }
}

void MacroAssembler::add64(Imm32 imm, Register64 dest) {
  ma_add_d(dest.reg, dest.reg, imm);
}

void MacroAssembler::add64(Imm64 imm, Register64 dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(dest.reg != scratch);
  mov(ImmWord(imm.value), scratch);
  as_add_d(dest.reg, dest.reg, scratch);
}

void MacroAssembler::add32(Register src, Register dest) {
  as_add_w(dest, dest, src);
}

void MacroAssembler::add32(Imm32 imm, Register dest) {
  ma_add_w(dest, dest, imm);
}

void MacroAssembler::add32(Imm32 imm, const Address& dest) {
  if (dest.base == SecondScratchReg) {
    ScratchRegisterScope scratch(asMasm());
    load32(dest, scratch);
    ma_add_w(scratch, scratch, imm);
    store32(scratch, dest);
    return;
  }

  SecondScratchRegisterScope scratch2(asMasm());
  load32(dest, scratch2);
  ma_add_w(scratch2, scratch2, imm);
  store32(scratch2, dest);
}

void MacroAssembler::addPtr(Imm32 imm, const Address& dest) {
  if (dest.base == ScratchRegister) {
    SecondScratchRegisterScope scratch2(asMasm());
    loadPtr(dest, scratch2);
    addPtr(imm, scratch2);
    storePtr(scratch2, dest);
    return;
  }

  ScratchRegisterScope scratch(asMasm());
  loadPtr(dest, scratch);
  addPtr(imm, scratch);
  storePtr(scratch, dest);
}

void MacroAssembler::addPtr(const Address& src, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  loadPtr(src, scratch);
  addPtr(scratch, dest);
}

void MacroAssembler::addDouble(FloatRegister src, FloatRegister dest) {
  as_fadd_d(dest, dest, src);
}

void MacroAssembler::addFloat32(FloatRegister src, FloatRegister dest) {
  as_fadd_s(dest, dest, src);
}
void MacroAssembler::subPtr(Register src, Register dest) {
  as_sub_d(dest, dest, src);
}

void MacroAssembler::subPtr(Imm32 imm, Register dest) {
  ma_sub_d(dest, dest, imm);
}

void MacroAssembler::sub64(Register64 src, Register64 dest) {
  as_sub_d(dest.reg, dest.reg, src.reg);
}

void MacroAssembler::sub64(const Operand& src, Register64 dest) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(asMasm());
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    sub64(scratch64, dest);
  } else {
    sub64(Register64(src.toReg()), dest);
  }
}

void MacroAssembler::sub64(Imm64 imm, Register64 dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(dest.reg != scratch);
  mov(ImmWord(imm.value), scratch);
  as_sub_d(dest.reg, dest.reg, scratch);
}

void MacroAssembler::sub32(Register src, Register dest) {
  as_sub_w(dest, dest, src);
}

void MacroAssembler::sub32(Imm32 imm, Register dest) {
  ma_sub_w(dest, dest, imm);
}

void MacroAssembler::sub32(const Address& src, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(src, scratch2);
  as_sub_w(dest, dest, scratch2);
}

void MacroAssembler::subPtr(Register src, const Address& dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(dest, scratch2);
  subPtr(src, scratch2);
  storePtr(scratch2, dest);
}

void MacroAssembler::subPtr(const Address& addr, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(addr, scratch2);
  subPtr(scratch2, dest);
}

void MacroAssembler::subDouble(FloatRegister src, FloatRegister dest) {
  as_fsub_d(dest, dest, src);
}

void MacroAssembler::subFloat32(FloatRegister src, FloatRegister dest) {
  as_fsub_s(dest, dest, src);
}

void MacroAssembler::mul64(Imm64 imm, const Register64& dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(dest.reg != scratch);
  mov(ImmWord(imm.value), scratch);
  as_mul_d(dest.reg, dest.reg, scratch);
}

void MacroAssembler::mul64(Imm64 imm, const Register64& dest,
                           const Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  mul64(imm, dest);
}

void MacroAssembler::mul64(const Register64& src, const Register64& dest,
                           const Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  as_mul_d(dest.reg, dest.reg, src.reg);
}

void MacroAssembler::mul64(const Operand& src, const Register64& dest,
                           const Register temp) {
  if (src.getTag() == Operand::MEM) {
    ScratchRegisterScope scratch(asMasm());
    Register64 scratch64(scratch);

    load64(src.toAddress(), scratch64);
    mul64(scratch64, dest, temp);
  } else {
    mul64(Register64(src.toReg()), dest, temp);
  }
}
void MacroAssembler::mulBy3(Register src, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(src != scratch);
  as_add_d(scratch, src, src);
  as_add_d(dest, scratch, src);
}

void MacroAssembler::mul32(Register rhs, Register srcDest) {
  as_mul_w(srcDest, srcDest, rhs);
}

void MacroAssembler::mulFloat32(FloatRegister src, FloatRegister dest) {
  as_fmul_s(dest, dest, src);
}

void MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest) {
  as_fmul_d(dest, dest, src);
}

void MacroAssembler::mulDoublePtr(ImmPtr imm, Register temp,
                                  FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  movePtr(imm, scratch);
  loadDouble(Address(scratch, 0), fpscratch);
  mulDouble(fpscratch, dest);
}

void MacroAssembler::inc64(AbsoluteAddress dest) {
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());
  ma_li(scratch, ImmWord(uintptr_t(dest.addr)));
  as_ld_d(scratch2, scratch, 0);
  as_addi_d(scratch2, scratch2, 1);
  as_st_d(scratch2, scratch, 0);
}

void MacroAssembler::quotient32(Register rhs, Register srcDest,
                                bool isUnsigned) {
  if (isUnsigned) {
    as_div_wu(srcDest, srcDest, rhs);
  } else {
    as_div_w(srcDest, srcDest, rhs);
  }
}

void MacroAssembler::remainder32(Register rhs, Register srcDest,
                                 bool isUnsigned) {
  if (isUnsigned) {
    as_mod_wu(srcDest, srcDest, rhs);
  } else {
    as_mod_w(srcDest, srcDest, rhs);
  }
}

void MacroAssembler::divFloat32(FloatRegister src, FloatRegister dest) {
  as_fdiv_s(dest, dest, src);
}

void MacroAssembler::divDouble(FloatRegister src, FloatRegister dest) {
  as_fdiv_d(dest, dest, src);
}

void MacroAssembler::neg64(Register64 reg) { as_sub_d(reg.reg, zero, reg.reg); }
void MacroAssembler::neg32(Register reg) { as_sub_w(reg, zero, reg); }

void MacroAssembler::negateDouble(FloatRegister reg) { as_fneg_d(reg, reg); }

void MacroAssembler::negateFloat(FloatRegister reg) { as_fneg_s(reg, reg); }
void MacroAssembler::absFloat32(FloatRegister src, FloatRegister dest) {
  as_fabs_s(dest, src);
}

void MacroAssembler::absDouble(FloatRegister src, FloatRegister dest) {
  as_fabs_d(dest, src);
}

void MacroAssembler::sqrtFloat32(FloatRegister src, FloatRegister dest) {
  as_fsqrt_s(dest, src);
}

void MacroAssembler::sqrtDouble(FloatRegister src, FloatRegister dest) {
  as_fsqrt_d(dest, src);
}

void MacroAssembler::minFloat32(FloatRegister other, FloatRegister srcDest,
                                bool handleNaN) {
  minMaxFloat32(srcDest, other, handleNaN, false);
}

void MacroAssembler::minDouble(FloatRegister other, FloatRegister srcDest,
                               bool handleNaN) {
  minMaxDouble(srcDest, other, handleNaN, false);
}

void MacroAssembler::maxFloat32(FloatRegister other, FloatRegister srcDest,
                                bool handleNaN) {
  minMaxFloat32(srcDest, other, handleNaN, true);
}

void MacroAssembler::maxDouble(FloatRegister other, FloatRegister srcDest,
                               bool handleNaN) {
  minMaxDouble(srcDest, other, handleNaN, true);
}

// ===============================================================
// Shift functions

void MacroAssembler::lshift32(Register src, Register dest) {
  as_sll_w(dest, dest, src);
}

void MacroAssembler::lshift32(Imm32 imm, Register dest) {
  as_slli_w(dest, dest, imm.value % 32);
}
void MacroAssembler::lshift64(Register shift, Register64 dest) {
  as_sll_d(dest.reg, dest.reg, shift);
}

void MacroAssembler::lshift64(Imm32 imm, Register64 dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_slli_d(dest.reg, dest.reg, imm.value);
}

void MacroAssembler::lshiftPtr(Imm32 imm, Register dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_slli_d(dest, dest, imm.value);
}

void MacroAssembler::rshift32(Register src, Register dest) {
  as_srl_w(dest, dest, src);
}

void MacroAssembler::rshift32(Imm32 imm, Register dest) {
  as_srli_w(dest, dest, imm.value % 32);
}
void MacroAssembler::rshift32Arithmetic(Register src, Register dest) {
  as_sra_w(dest, dest, src);
}

void MacroAssembler::rshift32Arithmetic(Imm32 imm, Register dest) {
  as_srai_w(dest, dest, imm.value % 32);
}
void MacroAssembler::rshift64(Register shift, Register64 dest) {
  as_srl_d(dest.reg, dest.reg, shift);
}

void MacroAssembler::rshift64(Imm32 imm, Register64 dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_srli_d(dest.reg, dest.reg, imm.value);
}

void MacroAssembler::rshift64Arithmetic(Imm32 imm, Register64 dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_srai_d(dest.reg, dest.reg, imm.value);
}

void MacroAssembler::rshift64Arithmetic(Register shift, Register64 dest) {
  as_sra_d(dest.reg, dest.reg, shift);
}

void MacroAssembler::rshiftPtr(Imm32 imm, Register dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_srli_d(dest, dest, imm.value);
}

void MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest) {
  MOZ_ASSERT(0 <= imm.value && imm.value < 64);
  as_srai_d(dest, dest, imm.value);
}

// ===============================================================
// Rotation functions

void MacroAssembler::rotateLeft(Register count, Register input, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  as_sub_w(scratch, zero, count);
  as_rotr_w(dest, input, scratch);
}

void MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest) {
  as_rotri_w(dest, input, (32 - count.value) & 31);
}

void MacroAssembler::rotateLeft64(Register count, Register64 src,
                                  Register64 dest) {
  rotateLeft64(count, src, dest, Register::Invalid());
}

void MacroAssembler::rotateLeft64(Register count, Register64 src,
                                  Register64 dest, Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  ScratchRegisterScope scratch(asMasm());
  as_sub_d(scratch, zero, count);
  as_rotr_d(dest.reg, src.reg, scratch);
}

void MacroAssembler::rotateLeft64(Imm32 count, Register64 src, Register64 dest) {
  rotateLeft64(count, src, dest, Register::Invalid());
}

void MacroAssembler::rotateLeft64(Imm32 count, Register64 src, Register64 dest,
                                  Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  as_rotri_d(dest.reg, src.reg, (64 - count.value) & 63);
}

void MacroAssembler::rotateRight(Register count, Register input,
                                 Register dest) {
  as_rotr_w(dest, input, count);
}

void MacroAssembler::rotateRight(Imm32 count, Register input, Register dest) {
  as_rotri_w(dest, input, count.value & 31);
}

void MacroAssembler::rotateRight64(Register count, Register64 src,
                                   Register64 dest) {
  rotateRight64(count, src, dest, Register::Invalid());
}

void MacroAssembler::rotateRight64(Register count, Register64 src,
                                   Register64 dest, Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  as_rotr_d(dest.reg, src.reg, count);
}

void MacroAssembler::rotateRight64(Imm32 count, Register64 src, Register64 dest) {
  rotateRight64(count, src, dest, Register::Invalid());
}

void MacroAssembler::rotateRight64(Imm32 count, Register64 src, Register64 dest,
                                   Register temp) {
  MOZ_ASSERT(temp == Register::Invalid());
  as_rotri_d(dest.reg, src.reg, count.value & 63);
}

// Bit counting functions

void MacroAssembler::clz64(Register64 src, Register dest) {
  as_clz_d(dest, src.reg);
}

void MacroAssembler::ctz64(Register64 src, Register dest) {
  as_ctz_d(dest, src.reg);
}

void MacroAssembler::popcnt64(Register64 input, Register64 output,
                              Register tmp) {
  ScratchRegisterScope scratch(asMasm());
  as_or(output.reg, input.reg, zero);
  as_srai_d(tmp, input.reg, 1);
  ma_li(scratch, Imm32(0x55555555));
  as_bstrins_d(scratch, scratch, 63, 32);
  as_and(tmp, tmp, scratch);
  as_sub_d(output.reg, output.reg, tmp);
  as_srai_d(tmp, output.reg, 2);
  ma_li(scratch, Imm32(0x33333333));
  as_bstrins_d(scratch, scratch, 63, 32);
  as_and(output.reg, output.reg, scratch);
  as_and(tmp, tmp, scratch);
  as_add_d(output.reg, output.reg, tmp);
  as_srli_d(tmp, output.reg, 4);
  as_add_d(output.reg, output.reg, tmp);
  ma_li(scratch, Imm32(0xF0F0F0F));
  as_bstrins_d(scratch, scratch, 63, 32);
  as_and(output.reg, output.reg, scratch);
  ma_li(tmp, Imm32(0x1010101));
  as_bstrins_d(tmp, tmp, 63, 32);
  as_mul_d(output.reg, output.reg, tmp);
  as_srai_d(output.reg, output.reg, 56);
}

void MacroAssembler::clz32(Register src, Register dest, bool knownNotZero) {
  as_clz_w(dest, src);
}

void MacroAssembler::ctz32(Register src, Register dest, bool knownNotZero) {
  as_ctz_w(dest, src);
}

void MacroAssembler::popcnt32(Register input, Register output, Register tmp) {
  // Equivalent to GCC output of mozilla::CountPopulation32()
  as_or(output, input, zero);
  as_srai_w(tmp, input, 1);
  ma_and(tmp, tmp, Imm32(0x55555555));
  as_sub_w(output, output, tmp);
  as_srai_w(tmp, output, 2);
  ma_and(output, output, Imm32(0x33333333));
  ma_and(tmp, tmp, Imm32(0x33333333));
  as_add_w(output, output, tmp);
  as_srli_w(tmp, output, 4);
  as_add_w(output, output, tmp);
  ma_and(output, output, Imm32(0xF0F0F0F));
  as_slli_w(tmp, output, 8);
  as_add_w(output, output, tmp);
  as_slli_w(tmp, output, 16);
  as_add_w(output, output, tmp);
  as_srai_w(output, output, 24);
}

// ===============================================================
// Condition functions
// Also see below for specializations of cmpPtrSet.
template <typename T1, typename T2>
void MacroAssembler::cmp32Set(Condition cond, T1 lhs, T2 rhs, Register dest) {
  ma_cmp_set(dest, lhs, rhs, cond);
}
template <typename T1, typename T2>
void MacroAssembler::cmpPtrSet(Condition cond, T1 lhs, T2 rhs, Register dest) {
  ma_cmp_set(dest, lhs, rhs, cond);
}

// ===============================================================
// Branch functions
template <class L>
void MacroAssembler::branch32(Condition cond, Register lhs, Register rhs,
                              L label) {
  ma_b(lhs, rhs, label, cond);
}

template <class L>
void MacroAssembler::branch32(Condition cond, Register lhs, Imm32 imm,
                              L label) {
  ma_b(lhs, imm, label, cond);
}

void MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs,
                              Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  ma_b(scratch2, rhs, label, cond);
}

void MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs,
                              Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  ma_b(scratch2, rhs, label, cond);
}

void MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs,
                              Register rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  ma_b(scratch2, rhs, label, cond);
}

void MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs,
                              Imm32 rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  ma_b(scratch2, rhs, label, cond);
}

void MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs,
                              Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  ma_b(scratch2, rhs, label, cond);
}

void MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress addr,
                              Imm32 imm, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  load32(addr, scratch2);
  ma_b(scratch2, imm, label, cond);
}

void MacroAssembler::branch64(Condition cond, Register64 lhs, Imm64 val,
                              Label* success, Label* fail) {
  MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal ||
                 cond == Assembler::LessThan ||
                 cond == Assembler::LessThanOrEqual ||
                 cond == Assembler::GreaterThan ||
                 cond == Assembler::GreaterThanOrEqual ||
                 cond == Assembler::Below || cond == Assembler::BelowOrEqual ||
                 cond == Assembler::Above || cond == Assembler::AboveOrEqual,
             "other condition codes not supported");

  branchPtr(cond, lhs.reg, ImmWord(val.value), success);
  if (fail) {
    jump(fail);
  }
}

void MacroAssembler::branch64(Condition cond, Register64 lhs, Register64 rhs,
                              Label* success, Label* fail) {
  MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal ||
                 cond == Assembler::LessThan ||
                 cond == Assembler::LessThanOrEqual ||
                 cond == Assembler::GreaterThan ||
                 cond == Assembler::GreaterThanOrEqual ||
                 cond == Assembler::Below || cond == Assembler::BelowOrEqual ||
                 cond == Assembler::Above || cond == Assembler::AboveOrEqual,
             "other condition codes not supported");

  branchPtr(cond, lhs.reg, rhs.reg, success);
  if (fail) {
    jump(fail);
  }
}

void MacroAssembler::branch64(Condition cond, const Address& lhs, Imm64 val,
                              Label* label) {
  MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal,
             "other condition codes not supported");

  branchPtr(cond, lhs, ImmWord(val.value), label);
}

void MacroAssembler::branch64(Condition cond, const Address& lhs,
                              const Address& rhs, Register scratch,
                              Label* label) {
  MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal,
             "other condition codes not supported");
  MOZ_ASSERT(lhs.base != scratch);
  MOZ_ASSERT(rhs.base != scratch);

  loadPtr(rhs, scratch);
  branchPtr(cond, lhs, scratch, label);
}

template <class L>
void MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs,
                               L label) {
  ma_b(lhs, rhs, label, cond);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs,
                               Label* label) {
  ma_b(lhs, rhs, label, cond);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs,
                               Label* label) {
  ma_b(lhs, rhs, label, cond);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs,
                               Label* label) {
  ma_b(lhs, rhs, label, cond);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs,
                               Label* label) {
  ma_b(lhs, rhs, label, cond);
}

template <class L>
void MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs,
                               L label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs,
                               Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs,
                               Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs,
                               Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs,
                               Register rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs,
                               ImmWord rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, wasm::SymbolicAddress lhs,
                               Register rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchPtr(cond, scratch2, rhs, label);
}

template <typename T>
CodeOffsetJump MacroAssembler::branchPtrWithPatch(Condition cond, Register lhs,
                                                  T rhs,
                                                  RepatchLabel* label) {
  movePtr(rhs, ScratchRegister);
  Label skipJump;
  ma_b(lhs, ScratchRegister, &skipJump, InvertCondition(cond), ShortJump);
  CodeOffsetJump off = jumpWithPatch(label);
  bind(&skipJump);
  return off;
}

template <typename T>
CodeOffsetJump MacroAssembler::branchPtrWithPatch(Condition cond, Address lhs,
                                                  T rhs,
                                                  RepatchLabel* label) {
  loadPtr(lhs, SecondScratchReg);
  movePtr(rhs, ScratchRegister);
  Label skipJump;
  ma_b(SecondScratchReg, ScratchRegister, &skipJump, InvertCondition(cond),
       ShortJump);
  CodeOffsetJump off = jumpWithPatch(label);
  bind(&skipJump);
  return off;
}

void MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs,
                                      Register rhs, Label* label) {
  branchPtr(cond, lhs, rhs, label);
}

void MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs,
                                 FloatRegister rhs, Label* label) {
  ma_bc_s(lhs, rhs, label, cond);
}

void MacroAssembler::branchTruncateFloat32MaybeModUint32(FloatRegister src,
                                                         Register dest,
                                                         Label* fail) {
  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  as_ftintrz_l_s(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromDouble(fpscratch, dest);
  MOZ_ASSERT(Assembler::CauseV < 32);
  as_bstrpick_w(scratch, scratch, Assembler::CauseV, Assembler::CauseV);
  ma_b(scratch, Imm32(0), fail, Assembler::NotEqual);

  as_slli_w(dest, dest, 0);
}

void MacroAssembler::branchTruncateFloat32ToInt32(FloatRegister src,
                                                  Register dest, Label* fail) {
  convertFloat32ToInt32(src, dest, fail, false);
}

void MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs,
                                  FloatRegister rhs, Label* label) {
  ma_bc_d(lhs, rhs, label, cond);
}

void MacroAssembler::branchTruncateDoubleMaybeModUint32(FloatRegister src,
                                                        Register dest,
                                                        Label* fail) {
  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  as_ftintrz_l_d(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromDouble(fpscratch, dest);
  MOZ_ASSERT(Assembler::CauseV < 32);
  as_bstrpick_w(scratch, scratch, Assembler::CauseV, Assembler::CauseV);
  ma_b(scratch, Imm32(0), fail, Assembler::NotEqual);

  as_slli_w(dest, dest, 0);
}

void MacroAssembler::branchTruncateDoubleToInt32(FloatRegister src,
                                                 Register dest, Label* fail) {
  convertDoubleToInt32(src, dest, fail, false);
}

template <typename T, typename L>
void MacroAssembler::branchAdd32(Condition cond, T src, Register dest,
                                 L overflow) {
  switch (cond) {
    case Overflow:
      ma_add32TestOverflow(dest, dest, src, overflow);
      break;
    case CarryClear:
    case CarrySet:
      ma_add32TestCarry(cond, dest, dest, src, overflow);
      break;
    case Equal:
    case NotEqual:
    case Zero:
    case NonZero:
    case Signed:
    case NotSigned:
      add32(src, dest);
      ma_b(dest, Imm32(0), overflow, cond);
      break;
    default:
      MOZ_CRASH("Unexpected branchAdd32 condition");
  }
}

// the type of 'T src' maybe a Register, maybe a Imm32,depends on who call it.
template <typename T>
void MacroAssembler::branchSub32(Condition cond, T src, Register dest,
                                 Label* overflow) {
  switch (cond) {
    case Overflow:
      ma_sub32TestOverflow(dest, dest, src, overflow);
      break;
    case CarrySet:
    case CarryClear: {
      ScratchRegisterScope scratch(asMasm());
      SecondScratchRegisterScope scratch2(asMasm());
      Register negSrc = dest == scratch ? Register(scratch2) : Register(scratch);
      ma_sub_w(negSrc, zero, src);
      // a - b borrows iff a + (-b) does not carry.
      ma_add32TestCarry(cond == CarrySet ? CarryClear : CarrySet, dest, dest,
                        negSrc, overflow);
      break;
    }
    case Equal:
    case NotEqual:
    case NonZero:
    case Zero:
    case Signed:
    case NotSigned:
      sub32(src, dest);
      ma_b(dest, Imm32(0), overflow, cond);
      break;
    default:
      MOZ_CRASH("Unexpected branchSub32 condition");
  }
}
void MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs,
                                  Label* label) {
  subPtr(rhs, lhs);
  branchPtr(cond, lhs, Imm32(0), label);
}

template <class L>
void MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs,
                                  L label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  if (lhs == rhs) {
    ma_b(lhs, rhs, label, cond);
  } else {
    ScratchRegisterScope scratch(asMasm());
    as_and(scratch, lhs, rhs);
    ma_b(scratch, scratch, label, cond);
  }
}

template <class L>
void MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs,
                                  L label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  SecondScratchRegisterScope scratch2(asMasm());
  ma_and(scratch2, lhs, rhs);
  ma_b(scratch2, scratch2, label, cond);
}

void MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs,
                                  Label* label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  and32(rhs, scratch2);
  ma_b(scratch2, scratch2, label, cond);
}

void MacroAssembler::branchTest32(Condition cond, const AbsoluteAddress& lhs,
                                  Imm32 rhs, Label* label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  SecondScratchRegisterScope scratch2(asMasm());
  load32(lhs, scratch2);
  and32(rhs, scratch2);
  ma_b(scratch2, scratch2, label, cond);
}

template <class L>
void MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs,
                                   L label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  if (lhs == rhs) {
    ma_b(lhs, rhs, label, cond);
  } else {
    ScratchRegisterScope scratch(asMasm());
    as_and(scratch, lhs, rhs);
    ma_b(scratch, scratch, label, cond);
  }
}

void MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs,
                                   Label* label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  ScratchRegisterScope scratch(asMasm());
  ma_and(scratch, lhs, rhs);
  ma_b(scratch, scratch, label, cond);
}

void MacroAssembler::branchTestPtr(Condition cond, const Address& lhs,
                                   Imm32 rhs, Label* label) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  branchTestPtr(cond, scratch2, rhs, label);
}

template <class L>
void MacroAssembler::branchTest64(Condition cond, Register64 lhs,
                                  Register64 rhs, Register temp, L label) {
  branchTestPtr(cond, lhs.reg, rhs.reg, label);
}

void MacroAssembler::branchTestUndefined(Condition cond, Register tag,
                                         Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_UNDEFINED), label, cond);
}

void MacroAssembler::branchTestUndefined(Condition cond,
                                         const ValueOperand& value,
                                         Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestUndefined(cond, scratch2, label);
}

void MacroAssembler::branchTestUndefined(Condition cond, const Address& address,
                                         Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestUndefined(cond, tag, label);
}

void MacroAssembler::branchTestUndefined(Condition cond,
                                         const BaseIndex& address,
                                         Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestUndefined(cond, tag, label);
}

void MacroAssembler::branchTestInt32(Condition cond, Register tag,
                                     Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_INT32), label, cond);
}

void MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value,
                                     Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestInt32(cond, scratch2, label);
}

void MacroAssembler::branchTestInt32(Condition cond, const Address& address,
                                     Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestInt32(cond, tag, label);
}

void MacroAssembler::branchTestInt32(Condition cond, const BaseIndex& address,
                                     Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestInt32(cond, tag, label);
}

void MacroAssembler::branchTestInt32Truthy(bool b, const ValueOperand& value,
                                           Label* label) {
  ScratchRegisterScope scratch(*this);
  as_bstrpick_d(scratch, value.valueReg(), 31, 0);
  ma_b(scratch, scratch, label, b ? NonZero : Zero);
}

void MacroAssembler::branchTestDouble(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  Condition actual = (cond == Equal) ? BelowOrEqual : Above;
  ma_b(tag, ImmTag(JSVAL_TAG_MAX_DOUBLE), label, actual);
}

void MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestDouble(cond, scratch2, label);
}

void MacroAssembler::branchTestDouble(Condition cond, const Address& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestDouble(cond, tag, label);
}

void MacroAssembler::branchTestDouble(Condition cond, const BaseIndex& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestDouble(cond, tag, label);
}

void MacroAssembler::branchTestDoubleTruthy(bool b, FloatRegister value,
                                            Label* label) {
  ScratchDoubleScope fpscratch(*this);
  ma_lid(fpscratch, 0.0);
  DoubleCondition cond = b ? DoubleNotEqual : DoubleEqualOrUnordered;
  ma_bc_d(value, fpscratch, label, cond);
}

void MacroAssembler::branchTestNumber(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  Condition actual = cond == Equal ? BelowOrEqual : Above;
  ma_b(tag, ImmTag(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET), label, actual);
}

void MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestNumber(cond, scratch2, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, Register tag,
                                       Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_BOOLEAN), label, cond);
}

void MacroAssembler::branchTestBoolean(Condition cond,
                                       const ValueOperand& value,
                                       Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestBoolean(cond, scratch2, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, const Address& address,
                                       Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestBoolean(cond, tag, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, const BaseIndex& address,
                                       Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestBoolean(cond, tag, label);
}

void MacroAssembler::branchTestBooleanTruthy(bool b, const ValueOperand& value,
                                             Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  unboxBoolean(value, scratch2);
  ma_b(scratch2, scratch2, label, b ? NonZero : Zero);
}

void MacroAssembler::branchTestString(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_STRING), label, cond);
}

void MacroAssembler::branchTestString(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestString(cond, scratch2, label);
}

void MacroAssembler::branchTestString(Condition cond, const BaseIndex& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestString(cond, tag, label);
}

void MacroAssembler::branchTestStringTruthy(bool b, const ValueOperand& value,
                                            Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  unboxString(value, scratch2);
  load32(Address(scratch2, JSString::offsetOfLength()), scratch2);
  ma_b(scratch2, Imm32(0), label, b ? NotEqual : Equal);
}

void MacroAssembler::branchTestSymbol(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_SYMBOL), label, cond);
}

void MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestSymbol(cond, scratch2, label);
}

void MacroAssembler::branchTestSymbol(Condition cond, const BaseIndex& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestSymbol(cond, tag, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_BIGINT), label, cond);
}

void MacroAssembler::branchTestBigInt(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestBigInt(cond, scratch2, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, const BaseIndex& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  computeEffectiveAddress(address, scratch2);
  splitTag(scratch2, scratch2);
  branchTestBigInt(cond, scratch2, label);
}

void MacroAssembler::branchTestBigIntTruthy(bool b, const ValueOperand& value,
                                            Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  unboxBigInt(value, scratch2);
  loadPtr(Address(scratch2, BigInt::offsetOfLengthSignAndReservedBits()), scratch2);
  ma_b(scratch2, ImmWord(0), label, b ? NotEqual : Equal);
}

void MacroAssembler::branchTestNull(Condition cond, Register tag,
                                    Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_NULL), label, cond);
}

void MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value,
                                    Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestNull(cond, scratch2, label);
}

void MacroAssembler::branchTestNull(Condition cond, const Address& address,
                                    Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestNull(cond, tag, label);
}

void MacroAssembler::branchTestNull(Condition cond, const BaseIndex& address,
                                    Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestNull(cond, tag, label);
}

void MacroAssembler::branchTestObject(Condition cond, Register tag,
                                      Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_OBJECT), label, cond);
}

void MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestObject(cond, scratch2, label);
}

void MacroAssembler::branchTestObject(Condition cond, const Address& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestObject(cond, tag, label);
}

void MacroAssembler::branchTestObject(Condition cond, const BaseIndex& address,
                                      Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestObject(cond, tag, label);
}

void MacroAssembler::branchTestPrimitive(Condition cond,
                                         const ValueOperand& value,
                                         Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  branchTestPrimitive(cond, scratch2, label);
}

void MacroAssembler::branchTestGCThing(Condition cond, const Address& address,
                                       Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  ma_b(tag, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET), label,
       (cond == Equal) ? AboveOrEqual : Below);
}

void MacroAssembler::branchTestGCThing(Condition cond, const BaseIndex& address,
                                       Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  ma_b(tag, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET), label,
       (cond == Equal) ? AboveOrEqual : Below);
}

void MacroAssembler::branchTestPrimitive(Condition cond, Register tag,
                                         Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET), label,
       (cond == Equal) ? Below : AboveOrEqual);
}

void MacroAssembler::branchTestMagic(Condition cond, Register tag,
                                     Label* label) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  ma_b(tag, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void MacroAssembler::branchTestMagic(Condition cond, const Address& address,
                                     Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestMagic(cond, tag, label);
}

void MacroAssembler::branchTestMagic(Condition cond, const BaseIndex& address,
                                     Label* label) {
  SecondScratchRegisterScope scratch2(*this);
  Register tag = extractTag(address, scratch2);
  branchTestMagic(cond, tag, label);
}

template <class L>
void MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value,
                                     L label) {
  SecondScratchRegisterScope scratch2(*this);
  splitTag(value, scratch2);
  ma_b(scratch2, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr,
                                     JSWhyMagic why, Label* label) {
  uint64_t magic = MagicValue(why).asRawBits();
  SecondScratchRegisterScope scratch(*this);
  loadPtr(valaddr, scratch);
  ma_b(scratch, ImmWord(magic), label, cond);
}

void MacroAssembler::storeUncanonicalizedDouble(FloatRegister src,
                                                const Address& addr) {
  ma_fst_d(src, addr);
}

void MacroAssembler::storeUncanonicalizedDouble(FloatRegister src,
                                                const BaseIndex& addr) {
  MOZ_ASSERT(addr.offset == 0);
  ma_fst_d(src, addr);
}

void MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src,
                                                 const Address& addr) {
  ma_fst_s(src, addr);
}

void MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src,
                                                 const BaseIndex& addr) {
  MOZ_ASSERT(addr.offset == 0);
  ma_fst_s(src, addr);
}

void MacroAssembler::clampIntToUint8(Register reg) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(reg != scratch);

  // Clamp semantics operate on int32 values; normalize the upper bits first in
  // case the producer left the high 32 bits undefined.
  as_slli_w(reg, reg, 0);

  as_slt(scratch, reg, zero);
  as_masknez(reg, reg, scratch);

  as_sltui(scratch, reg, 255);
  as_addi_d(reg, reg, -255);
  as_maskeqz(reg, reg, scratch);
  as_addi_d(reg, reg, 255);
}

template <class L>
void MacroAssembler::wasmBoundsCheck(Condition cond, Register index, L label) {
  BufferOffset bo = ma_BoundsCheck(ScratchRegister);
  append(wasm::BoundsCheck(bo.getOffset()));
  ma_b(index, ScratchRegister, label, cond);
}

void MacroAssembler::wasmPatchBoundsCheck(uint8_t* patchAt, uint32_t limit) {
  InstImm* i0 = reinterpret_cast<InstImm*>(patchAt);
  InstImm* i1 = reinterpret_cast<InstImm*>(i0->next());

  *i0 = InstImm(op_lu12i_w, int32_t((limit >> 12) & 0xfffff),
                Register::FromCode(i0->extractRD()), false);
  *i1 = InstImm(op_ori, int32_t(limit & 0xfff),
                Register::FromCode(i1->extractRJ()),
                Register::FromCode(i1->extractRD()), 12);
}

void MacroAssembler::memoryBarrier(MemoryBarrierBits barrier) {
  if (barrier == MembarNobits) {
    return;
  }
  as_dbar(0);
}

//}}} check_macroassembler_style
// ===============================================================

// The specializations for cmpPtrSet are outside the braces because
// check_macroassembler_style can't yet deal with specializations.

template <>
inline void MacroAssembler::cmpPtrSet(Assembler::Condition cond, Address lhs,
                                      ImmPtr rhs, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  loadPtr(lhs, scratch2);
  cmpPtrSet(cond, Register(scratch2), rhs, dest);
}

template <>
inline void MacroAssembler::cmpPtrSet(Assembler::Condition cond, Register lhs,
                                      Address rhs, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(lhs != scratch);
  loadPtr(rhs, scratch);
  cmpPtrSet(cond, lhs, Register(scratch), dest);
}

template <>
inline void MacroAssembler::cmpPtrSet(Assembler::Condition cond, Address lhs,
                                      Register rhs, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(rhs != scratch2);
  loadPtr(lhs, scratch2);
  cmpPtrSet(cond, Register(scratch2), rhs, dest);
}

template <>
inline void MacroAssembler::cmp32Set(Assembler::Condition cond, Register lhs,
                                     Address rhs, Register dest) {
  ScratchRegisterScope scratch(asMasm());
  MOZ_ASSERT(lhs != scratch);
  load32(rhs, scratch);
  cmp32Set(cond, lhs, Register(scratch), dest);
}

template <>
inline void MacroAssembler::cmp32Set(Assembler::Condition cond, Address lhs,
                                     Register rhs, Register dest) {
  SecondScratchRegisterScope scratch2(asMasm());
  MOZ_ASSERT(rhs != scratch2);
  load32(lhs, scratch2);
  cmp32Set(cond, Register(scratch2), rhs, dest);
}

void MacroAssemblerLOONGARCH64Compat::incrementInt32Value(const Address& addr) {
  asMasm().add32(Imm32(1), addr);
}

void MacroAssemblerLOONGARCH64Compat::retn(Imm32 n) {
  // pc <- [sp]; sp += n
  loadPtr(Address(StackPointer, 0), ra);
  asMasm().addPtr(n, StackPointer);
  as_jirl(zero, ra, BOffImm16(0));
}

// If source is a double, load into dest.
// If source is int32, convert to double and store in dest.
// Else, branch to failure.
void MacroAssemblerLOONGARCH64Compat::ensureDouble(const ValueOperand& source,
                                               FloatRegister dest,
                                               Label* failure) {
  Label isDouble, done;
  {
    ScratchTagScope tag(asMasm(), source);
    splitTagForTest(source, tag);
    asMasm().branchTestDouble(Assembler::Equal, tag, &isDouble);
    asMasm().branchTestInt32(Assembler::NotEqual, tag, failure);
  }

  convertInt32ToDouble(source.valueReg(), dest);
  jump(&done);

  bind(&isDouble);
  unboxDouble(source, dest);

  bind(&done);
}

}  // namespace jit
}  // namespace js

#endif /* jit_loongarch64_MacroAssembler_loongarch64_inl_h */
