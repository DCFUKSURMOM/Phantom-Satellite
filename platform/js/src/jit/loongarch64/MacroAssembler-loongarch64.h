/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_loongarch64_MacroAssembler_loongarch64_h
#define jit_loongarch64_MacroAssembler_loongarch64_h

#include "jit/loongarch64/Assembler-loongarch64.h"
#include "jit/MoveResolver.h"
#include "wasm/WasmTypes.h"

namespace js {
namespace jit {

enum LoadStoreSize {
  SizeByte = 8,
  SizeHalfWord = 16,
  SizeWord = 32,
  SizeDouble = 64
};

enum LoadStoreExtension { ZeroExtend = 0, SignExtend = 1 };

enum JumpKind { LongJump = 0, ShortJump = 1 };

static Register CallReg = t8;

enum LiFlags {
  Li64 = 0,
  Li48 = 1,
};

struct ImmShiftedTag : public ImmWord {
  explicit ImmShiftedTag(JSValueShiftedTag shtag) : ImmWord((uintptr_t)shtag) {}

  explicit ImmShiftedTag(JSValueType type)
      : ImmWord(uintptr_t(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type)))) {
  }
};

struct ImmTag : public Imm32 {
  ImmTag(JSValueTag mask) : Imm32(int32_t(mask)) {}
};

static const int defaultShift = 3;
static_assert(1 << defaultShift == sizeof(JS::Value),
              "The defaultShift is wrong");

// See documentation for ScratchTagScope and ScratchTagScopeRelease in
// MacroAssembler-x64.h.

class ScratchTagScope : public SecondScratchRegisterScope {
 public:
  ScratchTagScope(MacroAssembler& masm, const ValueOperand&)
      : SecondScratchRegisterScope(masm) {}

  void release() {}
  void reacquire() {}
};

class ScratchTagScopeRelease {
  ScratchTagScope* ts_;

 public:
  explicit ScratchTagScopeRelease(ScratchTagScope* ts) : ts_(ts) {
    ts_->release();
  }
  ~ScratchTagScopeRelease() { ts_->reacquire(); }
};

class MacroAssemblerLOONGARCH64 : public Assembler {
 protected:
  // Perform a downcast. Should be removed by Bug 996602.
  MacroAssembler& asMasm();
  const MacroAssembler& asMasm() const;

  Condition ma_cmp(Register rd, Register lhs, Register rhs, Condition c);
  Condition ma_cmp(Register rd, Register lhs, Imm32 imm, Condition c);

  void compareFloatingPoint(FloatFormat fmt, FloatRegister lhs,
                            FloatRegister rhs, DoubleCondition c,
                            FPConditionBit fcc = FCC0);

 public:
  void ma_li(Register dest, CodeOffset* label);
  void ma_li(Register dest, CodeLabel* label) { ma_li(dest, label->patchAt()); }
  void ma_li(Register dest, ImmWord imm);
  void ma_liPatchable(Register dest, ImmPtr imm);
  void ma_liPatchable(Register dest, ImmWord imm, LiFlags flags = Li48);

  // load
  void ma_ld_b(Register dest, Address address);
  void ma_ld_h(Register dest, Address address);
  void ma_ld_w(Register dest, Address address);
  void ma_ld_d(Register dest, Address address);
  void ma_ld_bu(Register dest, Address address);
  void ma_ld_hu(Register dest, Address address);
  void ma_ld_wu(Register dest, Address address);
  void ma_load(Register dest, Address address, LoadStoreSize size = SizeWord,
               LoadStoreExtension extension = SignExtend);

  // store
  void ma_st_b(Register src, Address address);
  void ma_st_h(Register src, Address address);
  void ma_st_w(Register src, Address address);
  void ma_st_d(Register src, Address address);
  void ma_store(Register data, Address address, LoadStoreSize size = SizeWord,
                LoadStoreExtension extension = SignExtend);

  // arithmetic based ops
  // add
  void ma_add_d(Register rd, Register rj, Imm32 imm);
  void ma_add32TestOverflow(Register rd, Register rj, Register rk,
                            Label* overflow);
  void ma_add32TestOverflow(Register rd, Register rj, Imm32 imm,
                            Label* overflow);
  template <typename L>
  void ma_add32TestOverflow(Register rd, Register rj, Register rk,
                            L overflow) {
    ScratchRegisterScope scratch(asMasm());
    as_add_d(scratch, rj, rk);
    as_add_w(rd, rj, rk);
    ma_b(rd, Register(scratch), overflow, Assembler::NotEqual);
  }
  template <typename L>
  void ma_add32TestOverflow(Register rd, Register rj, Imm32 imm, L overflow) {
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
  void ma_addPtrTestOverflow(Register rd, Register rj, Register rk,
                             Label* overflow);
  void ma_addPtrTestOverflow(Register rd, Register rj, Imm32 imm,
                             Label* overflow);
  void ma_addPtrTestOverflow(Register rd, Register rj, ImmWord imm,
                             Label* overflow);
  void ma_addPtrTestCarry(Condition cond, Register rd, Register rj, Register rk,
                          Label* overflow);
  void ma_addPtrTestCarry(Condition cond, Register rd, Register rj, Imm32 imm,
                          Label* overflow);
  void ma_addPtrTestCarry(Condition cond, Register rd, Register rj, ImmWord imm,
                          Label* overflow);

  // subtract
  void ma_sub_d(Register rd, Register rj, Imm32 imm);
  void ma_sub32TestOverflow(Register rd, Register rj, Register rk,
                            Label* overflow);
  void ma_subPtrTestOverflow(Register rd, Register rj, Register rk,
                             Label* overflow);
  void ma_subPtrTestOverflow(Register rd, Register rj, Imm32 imm,
                             Label* overflow);

  // multiplies.  For now, there are only few that we care about.
  void ma_mul_d(Register rd, Register rj, Imm32 imm);
  void ma_mulh_d(Register rd, Register rj, Imm32 imm);
  void ma_mulPtrTestOverflow(Register rd, Register rj, Register rk,
                             Label* overflow);

  // stack
  void ma_pop(Register r);
  void ma_push(Register r);

  void branchWithCode(InstImm code, Label* label, JumpKind jumpKind);
  // branches when done from within la-specific code
  void ma_b(Register lhs, ImmWord imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs, Address addr, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr, Imm32 imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr, ImmGCPtr imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr, Register rhs, Label* l, Condition c,
            JumpKind jumpKind = LongJump) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(rhs != scratch);
    ma_ld_d(scratch, addr);
    ma_b(scratch, rhs, l, c, jumpKind);
  }

  void ma_bl(Label* l);

  // fp instructions
  void ma_lid(FloatRegister dest, double value);

  void ma_mv(FloatRegister src, ValueOperand dest);
  void ma_mv(ValueOperand src, FloatRegister dest);

  void ma_fld_s(FloatRegister ft, Address address);
  void ma_fld_d(FloatRegister ft, Address address);
  void ma_fst_d(FloatRegister ft, Address address);
  void ma_fst_s(FloatRegister ft, Address address);

  void ma_pop(FloatRegister f);
  void ma_push(FloatRegister f);

  void ma_cmp_set(Register dst, Register lhs, ImmWord imm, Condition c);
  void ma_cmp_set(Register dst, Register lhs, ImmPtr imm, Condition c);
  void ma_cmp_set(Register dst, Address address, Imm32 imm, Condition c);
  void ma_cmp_set(Register dst, Address address, ImmWord imm, Condition c);

  void moveIfZero(Register dst, Register src, Register cond) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(dst != scratch && cond != scratch);
    as_masknez(scratch, src, cond);
    as_maskeqz(dst, dst, cond);
    as_or(dst, dst, scratch);
  }
  void moveIfNotZero(Register dst, Register src, Register cond) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(dst != scratch && cond != scratch);
    as_maskeqz(scratch, src, cond);
    as_masknez(dst, dst, cond);
    as_or(dst, dst, scratch);
  }

  // These functions abstract the access to high part of the double precision
  // float register. They are intended to work on both 32 bit and 64 bit
  // floating point coprocessor.
  void moveToDoubleHi(Register src, FloatRegister dest) {
    as_movgr2frh_w(dest, src);
  }
  void moveFromDoubleHi(FloatRegister src, Register dest) {
    as_movfrh2gr_s(dest, src);
  }

  void moveToDouble(Register src, FloatRegister dest) {
    as_movgr2fr_d(dest, src);
  }
  void moveFromDouble(FloatRegister src, Register dest) {
    as_movfr2gr_d(dest, src);
  }

 public:
  void ma_li(Register dest, ImmGCPtr ptr);

  void ma_li(Register dest, Imm32 imm);
  void ma_liPatchable(Register dest, Imm32 imm);

  void ma_rotr_w(Register rd, Register rj, Imm32 shift);

  void ma_fmovz(FloatFormat fmt, FloatRegister fd, FloatRegister fj,
                Register rk);
  void ma_fmovn(FloatFormat fmt, FloatRegister fd, FloatRegister fj,
                Register rk);

  void ma_and(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  void ma_or(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  void ma_xor(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  // load
  void ma_load(Register dest, const BaseIndex& src,
               LoadStoreSize size = SizeWord,
               LoadStoreExtension extension = SignExtend);

  // store
  void ma_store(Register data, const BaseIndex& dest,
                LoadStoreSize size = SizeWord,
                LoadStoreExtension extension = SignExtend);
  void ma_store(Imm32 imm, const BaseIndex& dest, LoadStoreSize size = SizeWord,
                LoadStoreExtension extension = SignExtend);

  // arithmetic based ops
  // add
  void ma_add_w(Register rd, Register rj, Imm32 imm);
  void ma_add32TestCarry(Condition cond, Register rd, Register rj, Register rk,
                         Label* overflow);
  void ma_add32TestCarry(Condition cond, Register rd, Register rj, Imm32 imm,
                         Label* overflow);
  template <typename L>
  void ma_add32TestCarry(Condition cond, Register rd, Register rj, Register rk,
                         L overflow) {
    MOZ_ASSERT(cond == Assembler::CarrySet || cond == Assembler::CarryClear);
    MOZ_ASSERT_IF(rd == rj, rk != rd);
    ScratchRegisterScope scratch(asMasm());
    as_add_w(rd, rj, rk);
    as_sltu(scratch, rd, rd == rj ? rk : rj);
    ma_b(Register(scratch), Register(scratch), overflow,
         cond == Assembler::CarrySet ? Assembler::NonZero : Assembler::Zero);
  }
  template <typename L>
  void ma_add32TestCarry(Condition cond, Register rd, Register rj, Imm32 imm,
                         L overflow) {
    SecondScratchRegisterScope scratch2(asMasm());
    MOZ_ASSERT(rj != scratch2);
    ma_li(scratch2, imm);
    ma_add32TestCarry(cond, rd, rj, scratch2, overflow);
  }
  template <typename L>
  void ma_addTestCarry(Register rd, Register rs, Register rt, L overflow) {
    MOZ_ASSERT_IF(rd == rs, rt != rd);
    SecondScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(scratch != rs);
    MOZ_ASSERT(scratch != rt);
    as_add_d(rd, rs, rt);
    as_sltu(scratch, rd, rd == rs ? rt : rs);
    ma_b(Register(scratch), Register(scratch), overflow, Assembler::NonZero);
  }
  template <typename L>
  void ma_addTestCarry(Register rd, Register rs, Imm32 imm, L overflow) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(scratch != rs);
    ma_li(scratch, imm);
    ma_addTestCarry(rd, rs, Register(scratch), overflow);
  }

  // subtract
  void ma_sub_w(Register rd, Register rj, Imm32 imm);
  void ma_sub_w(Register rd, Register rj, Register rk);
  void ma_sub32TestOverflow(Register rd, Register rj, Imm32 imm,
                            Label* overflow);

  // multiplies.  For now, there are only few that we care about.
  void ma_mul(Register rd, Register rj, Imm32 imm);
  void ma_mul_branch_overflow(Register rd, Register rj, Register rk,
                              Label* overflow) {
    ma_mul32TestOverflow(rd, rj, rk, overflow);
  }
  void ma_mul_branch_overflow(Register rd, Register rj, Imm32 imm,
                              Label* overflow) {
    ma_mul32TestOverflow(rd, rj, imm, overflow);
  }
  void ma_mul32TestOverflow(Register rd, Register rj, Register rk,
                            Label* overflow);
  void ma_mul32TestOverflow(Register rd, Register rj, Imm32 imm,
                            Label* overflow);

  // divisions
  void ma_div_branch_overflow(Register rd, Register rj, Register rk,
                              Label* overflow);
  void ma_div_branch_overflow(Register rd, Register rj, Imm32 imm,
                              Label* overflow);

  // fast mod, uses scratch registers, and thus needs to be in the assembler
  // implicitly assumes that we can overwrite dest at the beginning of the
  // sequence
  void ma_mod_mask(Register src, Register dest, Register hold, Register remain,
                   int32_t shift, Label* negZero = nullptr);

  void ma_and(Register rd, Register rs) { as_and(rd, rd, rs); }
  void ma_and(Register rd, Imm32 imm) { ma_and(rd, rd, imm); }
  void as_addu(Register rd, Register rj, Register rk) { as_add_w(rd, rj, rk); }
  void ma_addu(Register rd, Register rs) { as_add_w(rd, rd, rs); }
  void ma_addu(Register rd, Imm32 imm) { ma_add_w(rd, rd, imm); }
  void ma_addu(Register rd, Register rj, Register rk) { as_add_w(rd, rj, rk); }
  void ma_addu(Register rd, Register rj, Imm32 imm) { ma_add_w(rd, rj, imm); }
  void as_subu(Register rd, Register rj, Register rk) { as_sub_w(rd, rj, rk); }
  void ma_subu(Register rd, Register rs) { as_sub_w(rd, rd, rs); }
  void ma_subu(Register rd, Imm32 imm) { ma_sub_w(rd, rd, imm); }
  void ma_subu(Register rd, Register rj, Register rk) { as_sub_w(rd, rj, rk); }
  void ma_subu(Register rd, Register rj, Imm32 imm) { ma_sub_w(rd, rj, imm); }
  void ma_addTestOverflow(Register rd, Register rj, Register rk,
                          Label* overflow) {
    ma_add32TestOverflow(rd, rj, rk, overflow);
  }
  void ma_addTestOverflow(Register rd, Register rj, Imm32 imm,
                          Label* overflow) {
    ma_add32TestOverflow(rd, rj, imm, overflow);
  }
  void ma_subTestOverflow(Register rd, Register rj, Register rk,
                          Label* overflow) {
    ma_sub32TestOverflow(rd, rj, rk, overflow);
  }
  void ma_subTestOverflow(Register rd, Register rj, Imm32 imm,
                          Label* overflow) {
    ma_sub32TestOverflow(rd, rj, imm, overflow);
  }
  void ma_negu(Register rd, Register rj) { as_sub_w(rd, zero, rj); }
  void ma_not(Register rd, Register rj) { as_nor(rd, rj, zero); }
  void ma_or(Register rd, Register rs) { as_or(rd, rd, rs); }
  void ma_or(Register rd, Imm32 imm) { ma_or(rd, rd, imm); }
  void ma_xor(Register rd, Register rs) { as_xor(rd, rd, rs); }
  void ma_xor(Register rd, Imm32 imm) { ma_xor(rd, rd, imm); }
  void ma_sll(Register rd, Register rj, Register rk) { as_sll_w(rd, rj, rk); }
  void ma_sll(Register rd, Register rj, Imm32 imm) {
    as_slli_w(rd, rj, imm.value & 0x1f);
  }
  void ma_dsll(Register rd, Register rj, Register rk) { as_sll_d(rd, rj, rk); }
  void ma_dsll(Register rd, Register rj, Imm32 imm) {
    as_slli_d(rd, rj, imm.value & 0x3f);
  }
  void ma_srl(Register rd, Register rj, Register rk) { as_srl_w(rd, rj, rk); }
  void ma_srl(Register rd, Register rj, Imm32 imm) {
    as_srli_w(rd, rj, imm.value & 0x1f);
  }
  void ma_dsrl(Register rd, Register rj, Register rk) { as_srl_d(rd, rj, rk); }
  void ma_dsrl(Register rd, Register rj, Imm32 imm) {
    as_srli_d(rd, rj, imm.value & 0x3f);
  }
  void ma_sra(Register rd, Register rj, Register rk) { as_sra_w(rd, rj, rk); }
  void ma_sra(Register rd, Register rj, Imm32 imm) {
    as_srai_w(rd, rj, imm.value & 0x1f);
  }
  void ma_dsra(Register rd, Register rj, Register rk) { as_sra_d(rd, rj, rk); }
  void ma_dsra(Register rd, Register rj, Imm32 imm) {
    as_srai_d(rd, rj, imm.value & 0x3f);
  }
  void ma_dins(Register rd, Register rj, Imm32 pos, Imm32 size) {
    as_bstrins_d(rd, rj, pos.value + size.value - 1, pos.value);
  }
  void ma_dext(Register rd, Register rj, Imm32 pos, Imm32 size) {
    as_bstrpick_d(rd, rj, pos.value + size.value - 1, pos.value);
  }
  void as_clz(Register rd, Register rj) { as_clz_w(rd, rj); }
  void ma_ctz(Register rd, Register rj) { as_ctz_w(rd, rj); }

  // branches when done from within la-specific code
  void ma_b(Register lhs, Register rhs, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs, Imm32 imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs, ImmPtr imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs, ImmGCPtr imm, Label* l, Condition c,
            JumpKind jumpKind = LongJump) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(lhs != scratch);
    ma_li(scratch, imm);
    ma_b(lhs, scratch, l, c, jumpKind);
  }

  template <typename T>
  void ma_b(Register lhs, const T& rhs, wasm::TrapDesc target,
            Condition c, JumpKind jumpKind = LongJump) {
    Label label;
    ma_b(lhs, rhs, &label, c, jumpKind);
    bindLater(&label, target);
  }

  void ma_b(wasm::TrapDesc target, JumpKind jumpKind = LongJump) {
    Label label;
    ma_b(&label, jumpKind);
    bindLater(&label, target);
  }

  void ma_b(Label* l, JumpKind jumpKind = LongJump);

  // fp instructions
  void ma_lis(FloatRegister dest, float value);

  void ma_fst_d(FloatRegister src, BaseIndex address);
  void ma_fst_s(FloatRegister src, BaseIndex address);

  void ma_fld_d(FloatRegister dest, const BaseIndex& src);
  void ma_fld_s(FloatRegister dest, const BaseIndex& src);

  // FP branches
  void ma_bc_s(FloatRegister lhs, FloatRegister rhs, Label* label,
               DoubleCondition c, JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0);
  void ma_bc_d(FloatRegister lhs, FloatRegister rhs, Label* label,
               DoubleCondition c, JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0);
  void ma_bc1s(FloatRegister lhs, FloatRegister rhs, Label* label,
               DoubleCondition c, JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0) {
    ma_bc_s(lhs, rhs, label, c, jumpKind, fcc);
  }
  void ma_bc1d(FloatRegister lhs, FloatRegister rhs, Label* label,
               DoubleCondition c, JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0) {
    ma_bc_d(lhs, rhs, label, c, jumpKind, fcc);
  }

  void ma_call(ImmPtr dest);

  void ma_jump(ImmPtr dest);

  void as_mul(Register rd, Register rj, Register rk) { as_mul_w(rd, rj, rk); }
  void as_movz(Register rd, Register rs, Register rt) { moveIfZero(rd, rs, rt); }
  void as_movz(FloatFormat fmt, FloatRegister fd, FloatRegister fs,
               Register rt) {
    ma_fmovz(fmt, fd, fs, rt);
  }
  void as_mtc1(Register rt, FloatRegister fs) { as_movgr2fr_w(fs, rt); }
  void as_mfc1(Register rt, FloatRegister fs) { as_movfr2gr_s(rt, fs); }
  void as_dmtc1(Register rt, FloatRegister fs) { as_movgr2fr_d(fs, rt); }
  void as_dmfc1(Register rt, FloatRegister fs) { as_movfr2gr_d(rt, fs); }
  BufferOffset as_cfc1(Register rd, FPControl) { return as_movfcsr2gr(rd); }
  BufferOffset as_ext(Register rd, Register rj, int32_t pos, int32_t size) {
    return as_bstrpick_w(rd, rj, pos + size - 1, pos);
  }
  BufferOffset as_ins(Register rd, Register rj, int32_t pos, int32_t size) {
    return as_bstrins_w(rd, rj, pos + size - 1, pos);
  }
  BufferOffset as_sd(Register rd, Register rj, int32_t si12) {
    return as_st_d(rd, rj, si12);
  }
  BufferOffset as_sd(FloatRegister fd, Register rj, int32_t si12) {
    return as_fst_d(fd, rj, si12);
  }
  BufferOffset as_ld(FloatRegister fd, Register rj, int32_t si12) {
    return as_fld_d(fd, rj, si12);
  }
  BufferOffset as_absd(FloatRegister fd, FloatRegister fj) {
    return as_fabs_d(fd, fj);
  }
  BufferOffset as_abss(FloatRegister fd, FloatRegister fj) {
    return as_fabs_s(fd, fj);
  }
  BufferOffset as_addd(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fadd_d(fd, fj, fk);
  }
  BufferOffset as_adds(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fadd_s(fd, fj, fk);
  }
  BufferOffset as_subd(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fsub_d(fd, fj, fk);
  }
  BufferOffset as_subs(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fsub_s(fd, fj, fk);
  }
  BufferOffset as_muld(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fmul_d(fd, fj, fk);
  }
  BufferOffset as_muls(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fmul_s(fd, fj, fk);
  }
  BufferOffset as_divd(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fdiv_d(fd, fj, fk);
  }
  BufferOffset as_divs(FloatRegister fd, FloatRegister fj, FloatRegister fk) {
    return as_fdiv_s(fd, fj, fk);
  }
  BufferOffset as_negd(FloatRegister fd, FloatRegister fj) {
    return as_fneg_d(fd, fj);
  }
  BufferOffset as_negs(FloatRegister fd, FloatRegister fj) {
    return as_fneg_s(fd, fj);
  }
  BufferOffset as_sqrtd(FloatRegister fd, FloatRegister fj) {
    return as_fsqrt_d(fd, fj);
  }
  BufferOffset as_sqrts(FloatRegister fd, FloatRegister fj) {
    return as_fsqrt_s(fd, fj);
  }
  BufferOffset as_floorwd(FloatRegister fd, FloatRegister fj) {
    return as_ftintrm_w_d(fd, fj);
  }
  BufferOffset as_floorws(FloatRegister fd, FloatRegister fj) {
    return as_ftintrm_w_s(fd, fj);
  }
  BufferOffset as_ceilwd(FloatRegister fd, FloatRegister fj) {
    return as_ftintrp_w_d(fd, fj);
  }
  BufferOffset as_ceilws(FloatRegister fd, FloatRegister fj) {
    return as_ftintrp_w_s(fd, fj);
  }
  BufferOffset as_truncld(FloatRegister fd, FloatRegister fj) {
    return as_ftintrz_l_d(fd, fj);
  }
  BufferOffset as_truncls(FloatRegister fd, FloatRegister fj) {
    return as_ftintrz_l_s(fd, fj);
  }
  BufferOffset as_truncwd(FloatRegister fd, FloatRegister fj) {
    return as_ftintrz_w_d(fd, fj);
  }
  BufferOffset as_truncws(FloatRegister fd, FloatRegister fj) {
    return as_ftintrz_w_s(fd, fj);
  }
  BufferOffset ma_BoundsCheck(Register bounded) {
    BufferOffset bo = nextOffset();
    ma_liPatchable(bounded, Imm32(0));
    return bo;
  }
  void ma_load_unaligned(const wasm::MemoryAccessDesc& access, Register dest,
                         const BaseIndex& src, Register temp,
                         LoadStoreSize size, LoadStoreExtension extension);
  void ma_store_unaligned(const wasm::MemoryAccessDesc& access, Register data,
                          const BaseIndex& dest, Register temp,
                          LoadStoreSize size, LoadStoreExtension extension);
  void loadUnalignedDouble(const wasm::MemoryAccessDesc& access,
                           const BaseIndex& src, Register temp,
                           FloatRegister dest);
  void loadUnalignedFloat32(const wasm::MemoryAccessDesc& access,
                            const BaseIndex& src, Register temp,
                            FloatRegister dest);
  void storeUnalignedDouble(const wasm::MemoryAccessDesc& access,
                            FloatRegister src, Register temp,
                            const BaseIndex& dest);
  void storeUnalignedFloat32(const wasm::MemoryAccessDesc& access,
                             FloatRegister src, Register temp,
                             const BaseIndex& dest);

  void ma_cmp_set(Register dst, Register lhs, Register rhs, Condition c);
  void ma_cmp_set(Register dst, Register lhs, Imm32 imm, Condition c);
  void ma_cmp_set_double(Register dst, FloatRegister lhs, FloatRegister rhs,
                         DoubleCondition c);
  void ma_cmp_set_float32(Register dst, FloatRegister lhs, FloatRegister rhs,
                          DoubleCondition c);

  void moveToDoubleLo(Register src, FloatRegister dest) {
    as_movgr2fr_w(dest, src);
  }
  void moveFromDoubleLo(FloatRegister src, Register dest) {
    as_movfr2gr_s(dest, src);
  }

  void moveToFloat32(Register src, FloatRegister dest) {
    as_movgr2fr_w(dest, src);
  }
  void moveFromFloat32(FloatRegister src, Register dest) {
    as_movfr2gr_s(dest, src);
  }

  // Evaluate srcDest = minmax<isMax>{Float32,Double}(srcDest, other).
  // Handle NaN specially if handleNaN is true.
  void minMaxDouble(FloatRegister srcDest, FloatRegister other, bool handleNaN,
                    bool isMax);
  void minMaxFloat32(FloatRegister srcDest, FloatRegister other, bool handleNaN,
                     bool isMax);

  void loadDouble(const Address& addr, FloatRegister dest);
  void loadDouble(const BaseIndex& src, FloatRegister dest);

  // Load a float value into a register, then expand it to a double.
  void loadFloatAsDouble(const Address& addr, FloatRegister dest);
  void loadFloatAsDouble(const BaseIndex& src, FloatRegister dest);

  void loadFloat32(const Address& addr, FloatRegister dest);
  void loadFloat32(const BaseIndex& src, FloatRegister dest);


 protected:
  void wasmLoadImpl(const wasm::MemoryAccessDesc& access, Register memoryBase,
                    Register ptr, Register ptrScratch, AnyRegister output,
                    Register tmp);
  void wasmStoreImpl(const wasm::MemoryAccessDesc& access, AnyRegister value,
                     Register memoryBase, Register ptr, Register ptrScratch,
                     Register tmp);
};

class MacroAssembler;

class MacroAssemblerLOONGARCH64Compat : public MacroAssemblerLOONGARCH64 {
 private:
  enum PendingDivKind {
    PendingDivNone,
    PendingDivW,
    PendingDivWU,
    PendingDivD,
    PendingDivDU
  };

  PendingDivKind pendingDivKind_;
  Register pendingDivLhs_;
  Register pendingDivRhs_;

 public:
  using MacroAssemblerLOONGARCH64::call;

  MacroAssemblerLOONGARCH64Compat()
      : pendingDivKind_(PendingDivNone),
        pendingDivLhs_(InvalidReg),
        pendingDivRhs_(InvalidReg) {}

  struct AutoPrepareForPatching {
    explicit AutoPrepareForPatching(MacroAssemblerLOONGARCH64Compat&) {}
  };

  bool asmMergeWith(const MacroAssemblerLOONGARCH64Compat& other) {
    if (!AssemblerShared::asmMergeWith(size(), other))
      return false;
    return m_buffer.appendBuffer(other.m_buffer);
  }

  size_t labelToPatchOffset(CodeOffset label) { return label.offset(); }

  void convertBoolToInt32(Register src, Register dest) {
    ma_and(dest, src, Imm32(0xff));
  };
  void convertInt32ToDouble(Register src, FloatRegister dest) {
    as_movgr2fr_w(dest, src);
    as_ffint_d_w(dest, dest);
  };
  void convertInt32ToDouble(const Address& src, FloatRegister dest) {
    ma_fld_s(dest, src);
    as_ffint_d_w(dest, dest);
  };
  void convertInt32ToDouble(const BaseIndex& src, FloatRegister dest) {
    ScratchRegisterScope scratch(asMasm());
    MOZ_ASSERT(scratch != src.base);
    MOZ_ASSERT(scratch != src.index);
    computeScaledAddress(src, scratch);
    convertInt32ToDouble(Address(scratch, src.offset), dest);
  };
  void convertUInt32ToDouble(Register src, FloatRegister dest);
  void convertInt64ToDouble(Register src, FloatRegister dest);
  void convertInt64ToFloat32(Register src, FloatRegister dest);
  void convertUInt32ToFloat32(Register src, FloatRegister dest);
  void convertDoubleToFloat32(FloatRegister src, FloatRegister dest);
  void convertUInt64ToFloat32(Register src, FloatRegister dest);
  void convertDoubleToInt32(FloatRegister src, Register dest, Label* fail,
                            bool negativeZeroCheck = true);
  void convertDoubleToPtr(FloatRegister src, Register dest, Label* fail,
                          bool negativeZeroCheck = true);
  void convertFloat32ToInt32(FloatRegister src, Register dest, Label* fail,
                             bool negativeZeroCheck = true);

  void convertFloat32ToDouble(FloatRegister src, FloatRegister dest);
  void convertInt32ToFloat32(Register src, FloatRegister dest);
  void convertInt32ToFloat32(const Address& src, FloatRegister dest);

  void movq(Register rj, Register rd);

  void computeScaledAddress(const BaseIndex& address, Register dest);

  void computeEffectiveAddress(const Address& address, Register dest) {
    ma_add_d(dest, address.base, Imm32(address.offset));
  }

  void computeEffectiveAddress(const BaseIndex& address, Register dest) {
    computeScaledAddress(address, dest);
    if (address.offset) {
      ma_add_d(dest, dest, Imm32(address.offset));
    }
  }

  void j(Label* dest) { ma_b(dest); }

  void mov(Register src, Register dest) { as_ori(dest, src, 0); }
  void mov(ImmWord imm, Register dest) { ma_li(dest, imm); }
  void mov(ImmPtr imm, Register dest) {
    mov(ImmWord(uintptr_t(imm.value)), dest);
  }
  void mov(CodeLabel* label, Register dest) { ma_li(dest, label); }
  void mov(Register src, Address dest) { storePtr(src, dest); }
  void mov(Address src, Register dest) { loadPtr(src, dest); }
  BufferOffset as_div(Register lhs, Register rhs);
  BufferOffset as_divu(Register lhs, Register rhs);
  BufferOffset as_ddiv(Register lhs, Register rhs);
  BufferOffset as_ddivu(Register lhs, Register rhs);
  BufferOffset as_mflo(Register dest);
  BufferOffset as_mfhi(Register dest);

  void writeDataRelocation(const Value& val) {
    // Raw GC pointer relocations and Value relocations both end up in
    // TraceOneDataRelocation.
    if (val.isGCThing()) {
      gc::Cell* cell = val.toGCThing();
      if (cell && gc::IsInsideNursery(cell)) {
        embedsNurseryPointers_ = true;
      }
      dataRelocations_.writeUnsigned(currentOffset());
    }
  }

  void branch(JitCode* c) {
    ScratchRegisterScope scratch(asMasm());
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, ImmPtr(c->raw()), Relocation::JITCODE);
    ma_liPatchable(scratch, ImmPtr(c->raw()));
    as_jirl(zero, scratch, BOffImm16(0));
  }
  void branch(const Register reg) { as_jirl(zero, reg, BOffImm16(0)); }
  void nop() { as_nop(); }
  void ret() {
    ma_pop(ra);
    as_jirl(zero, ra, BOffImm16(0));
  }
  inline void retn(Imm32 n);
  void push(Imm32 imm) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(ImmWord imm) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(ImmGCPtr imm) {
    ScratchRegisterScope scratch(asMasm());
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(const Address& address) {
    SecondScratchRegisterScope scratch2(asMasm());
    loadPtr(address, scratch2);
    ma_push(scratch2);
  }
  void push(Register reg) { ma_push(reg); }
  void push(FloatRegister reg) { ma_push(reg); }
  void pop(Register reg) { ma_pop(reg); }
  void pop(FloatRegister reg) { ma_pop(reg); }
  void pop(const Address& address) {
    ScratchRegisterScope scratch(asMasm());
    ma_pop(scratch);
    storePtr(scratch, address);
  }

  // Emit a branch that can be toggled to a non-operation. On LOONGARCH64 we use
  // "andi" instruction to toggle the branch.
  // See ToggleToJmp(), ToggleToCmp().
  CodeOffset toggledJump(Label* label);

  // Emit a "jalr" or "nop" instruction. ToggleCall can be used to patch
  // this instruction.
  CodeOffset toggledCall(JitCode* target, bool enabled);

  static size_t ToggledCallSize(uint8_t* code) {
    // Four instructions used in: MacroAssemblerLOONGARCH64Compat::toggledCall
    return 4 * sizeof(uint32_t);
  }

  CodeOffset pushWithPatch(ImmWord imm) {
    ScratchRegisterScope scratch(asMasm());
    CodeOffset offset = movWithPatch(imm, scratch);
    ma_push(scratch);
    return offset;
  }

  CodeOffset movWithPatch(ImmWord imm, Register dest) {
    CodeOffset offset = CodeOffset(currentOffset());
    ma_liPatchable(dest, imm, Li64);
    return offset;
  }
  CodeOffset movWithPatch(ImmPtr imm, Register dest) {
    CodeOffset offset = CodeOffset(currentOffset());
    ma_liPatchable(dest, imm);
    return offset;
  }

  void writeCodePointer(CodeLabel* label) {
    label->patchAt()->bind(currentOffset());
    m_buffer.ensureSpace(sizeof(void*));
    writeInst(-1);
    writeInst(-1);
  }

  void jump(Label* label) { ma_b(label); }
  void jump(Register reg) { as_jirl(zero, reg, BOffImm16(0)); }
  void jump(const Address& address) {
    ScratchRegisterScope scratch(asMasm());
    loadPtr(address, scratch);
    as_jirl(zero, scratch, BOffImm16(0));
  }

  void jump(JitCode* code) { branch(code); }
  void jump(wasm::TrapDesc target) { ma_b(target); }

  void jump(ImmPtr ptr) {
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, ptr, Relocation::HARDCODED);
    ma_jump(ptr);
  }

  CodeOffsetJump backedgeJump(RepatchLabel* label, Label* documentation = nullptr);
  CodeOffsetJump jumpWithPatch(RepatchLabel* label, Label* documentation = nullptr);

  void splitTag(Register src, Register dest) {
    as_srli_d(dest, src, JSVAL_TAG_SHIFT);
  }

  void splitTag(const ValueOperand& operand, Register dest) {
    splitTag(operand.valueReg(), dest);
  }

  Register splitTagForTest(const ValueOperand& value) {
    splitTag(value, SecondScratchReg);
    return SecondScratchReg;
  }

  void splitTagForTest(const ValueOperand& value, ScratchTagScope& tag) {
    splitTag(value, tag);
  }

  // unboxing code
  void unboxNonDouble(const ValueOperand& operand, Register dest);
  void unboxNonDouble(const Address& src, Register dest);
  void unboxNonDouble(const BaseIndex& src, Register dest);

  void unboxNonDouble(const ValueOperand& operand, Register dest,
                      JSValueType type) {
    unboxNonDouble(operand.valueReg(), dest, type);
  }

  template <typename T>
  void unboxNonDouble(T src, Register dest, JSValueType type) {
    MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      load32(src, dest);
      return;
    }
    loadPtr(src, dest);
    unboxNonDouble(dest, dest, type);
  }

  void unboxNonDouble(Register src, Register dest, JSValueType type) {
    MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      as_slli_w(dest, src, 0);
      return;
    }

    as_bstrpick_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
  }

  template <typename T>
  void unboxObjectOrNull(const T& src, Register dest) {
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
    // Object and null differ by a single tag bit in the boxed value.
    as_bstrins_d(dest, zero, JSVAL_TAG_SHIFT + 3, JSVAL_TAG_SHIFT + 3);
  }

  void unboxGCThingForGCBarrier(const Address& src, Register dest) {
    loadPtr(src, dest);
    as_bstrpick_d(dest, dest, JSVAL_TAG_SHIFT - 1, 0);
  }
  void unboxGCThingForGCBarrier(const ValueOperand& src, Register dest) {
    as_bstrpick_d(dest, src.valueReg(), JSVAL_TAG_SHIFT - 1, 0);
  }

  void unboxInt32(const ValueOperand& operand, Register dest);
  void unboxInt32(Register src, Register dest);
  void unboxInt32(const Address& src, Register dest);
  void unboxInt32(const BaseIndex& src, Register dest);
  void unboxBoolean(const ValueOperand& operand, Register dest);
  void unboxBoolean(Register src, Register dest);
  void unboxBoolean(const Address& src, Register dest);
  void unboxBoolean(const BaseIndex& src, Register dest);
  void unboxDouble(const ValueOperand& operand, FloatRegister dest);
  void unboxDouble(Register src, Register dest);
  void unboxDouble(const Address& src, FloatRegister dest);
  void unboxDouble(const BaseIndex& src, FloatRegister dest);
  void unboxString(const ValueOperand& operand, Register dest);
  void unboxString(Register src, Register dest);
  void unboxString(const Address& src, Register dest);
  void unboxSymbol(const ValueOperand& src, Register dest);
  void unboxSymbol(Register src, Register dest);
  void unboxSymbol(const Address& src, Register dest);
  void unboxBigInt(const ValueOperand& operand, Register dest);
  void unboxBigInt(Register src, Register dest);
  void unboxBigInt(const Address& src, Register dest);
  void unboxObject(const ValueOperand& src, Register dest);
  void unboxObject(Register src, Register dest);
  void unboxObject(const Address& src, Register dest);
  void unboxObject(const BaseIndex& src, Register dest) {
    unboxNonDouble(src, dest);
  }
  void unboxValue(const ValueOperand& src, AnyRegister dest);

  void notBoolean(const ValueOperand& val) {
    as_xori(val.valueReg(), val.valueReg(), 1);
  }

  // boxing code
  void boxDouble(FloatRegister src, const ValueOperand& dest, FloatRegister);
  void boxDouble(FloatRegister src, const ValueOperand& dest) {
    boxDouble(src, dest, ScratchDoubleReg);
  }
  void boxNonDouble(JSValueType type, Register src, const ValueOperand& dest);

  // Extended unboxing API. If the payload is already in a register, returns
  // that register. Otherwise, provides a move to the given scratch register,
  // and returns that.
  [[nodiscard]] Register extractObject(const Address& address,
                                       Register scratch);
  [[nodiscard]] Register extractString(const Address& address,
                                       Register scratch) {
    unboxString(address, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractObject(const ValueOperand& value,
                                       Register scratch) {
    unboxObject(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractString(const ValueOperand& value,
                                       Register scratch) {
    unboxString(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractSymbol(const ValueOperand& value,
                                       Register scratch) {
    unboxSymbol(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractInt32(const ValueOperand& value,
                                      Register scratch) {
    unboxInt32(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractBoolean(const ValueOperand& value,
                                        Register scratch) {
    unboxBoolean(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractTag(const Address& address, Register scratch);
  [[nodiscard]] Register extractTag(const BaseIndex& address, Register scratch);
  [[nodiscard]] Register extractTag(const ValueOperand& value,
                                    Register scratch) {
    splitTag(value, scratch);
    return scratch;
  }

  inline void ensureDouble(const ValueOperand& source, FloatRegister dest,
                           Label* failure);

  void boolValueToDouble(const ValueOperand& operand, FloatRegister dest);
  void int32ValueToDouble(const ValueOperand& operand, FloatRegister dest);
  void loadInt32OrDouble(const Address& src, FloatRegister dest);
  void loadInt32OrDouble(const BaseIndex& addr, FloatRegister dest);
  void loadConstantDouble(double dp, FloatRegister dest);
  void loadConstantDouble(wasm::RawF64 dp, FloatRegister dest) {
    loadConstantDouble(dp.fp(), dest);
  }

  void boolValueToFloat32(const ValueOperand& operand, FloatRegister dest);
  void int32ValueToFloat32(const ValueOperand& operand, FloatRegister dest);
  void loadConstantFloat32(float f, FloatRegister dest);
  void loadConstantFloat32(wasm::RawF32 f, FloatRegister dest) {
    loadConstantFloat32(f.fp(), dest);
  }

  void testNullSet(Condition cond, const ValueOperand& value, Register dest);

  void testObjectSet(Condition cond, const ValueOperand& value, Register dest);

  void testUndefinedSet(Condition cond, const ValueOperand& value,
                        Register dest);

  // higher level tag testing code
  Address ToPayload(Address value) { return value; }

  void moveValue(const Value& val, Register dest);
  void moveValue(const TypedOrValueRegister& src, const ValueOperand& dest);
  void moveValue(const Value& src, const ValueOperand& dest);
  void moveValue(const ValueOperand& src, const ValueOperand& dest);

  template <typename T>
  void loadUnboxedValue(const T& address, MIRType type, AnyRegister dest) {
    if (dest.isFloat()) {
      loadInt32OrDouble(address, dest.fpu());
    } else {
      unboxNonDouble(address, dest.gpr(), ValueTypeFromMIRType(type));
    }
  }

  template <typename T>
  void storeUnboxedPayload(ValueOperand value, T address, size_t nbytes) {
    switch (nbytes) {
      case 8:
        unboxNonDouble(value, ScratchRegister);
        storePtr(ScratchRegister, address);
        return;
      case 4:
        store32(value.valueReg(), address);
        return;
      case 1:
        store8(value.valueReg(), address);
        return;
      default:
        MOZ_CRASH("Bad payload width");
    }
  }

  void storeUnboxedPayload(ValueOperand value, BaseIndex address, size_t nbytes,
                           JSValueType type) {
    switch (nbytes) {
      case 8: {
        ScratchRegisterScope scratch(asMasm());
        SecondScratchRegisterScope scratch2(asMasm());
        if (type == JSVAL_TYPE_OBJECT) {
          unboxObjectOrNull(value, scratch2);
        } else {
          unboxNonDouble(value, scratch2, type);
        }
        computeEffectiveAddress(address, scratch);
        as_st_d(scratch2, scratch, 0);
        return;
      }
      case 4:
        store32(value.valueReg(), address);
        return;
      case 1:
        store8(value.valueReg(), address);
        return;
      default:
        MOZ_CRASH("Bad payload width");
    }
  }

  void storeUnboxedPayload(ValueOperand value, Address address, size_t nbytes,
                           JSValueType type) {
    switch (nbytes) {
      case 8: {
        SecondScratchRegisterScope scratch2(asMasm());
        if (type == JSVAL_TYPE_OBJECT) {
          unboxObjectOrNull(value, scratch2);
        } else {
          unboxNonDouble(value, scratch2, type);
        }
        storePtr(scratch2, address);
        return;
      }
      case 4:
        store32(value.valueReg(), address);
        return;
      case 1:
        store8(value.valueReg(), address);
        return;
      default:
        MOZ_CRASH("Bad payload width");
    }
  }

  void boxValue(JSValueType type, Register src, Register dest) {
    ScratchRegisterScope scratch(asMasm());
    if (src == dest) {
      as_ori(scratch, src, 0);
      src = scratch;
    }
#ifdef DEBUG
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      Label upper32BitsSignExtended;
      as_slli_w(dest, src, 0);
      ma_b(src, dest, &upper32BitsSignExtended, Equal, ShortJump);
      breakpoint();
      bind(&upper32BitsSignExtended);
    }
#endif
    ma_li(dest, Imm32(int32_t(JSVAL_TYPE_TO_TAG(type))));
    as_slli_d(dest, dest, JSVAL_TAG_SHIFT);
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      as_bstrins_d(dest, src, 31, 0);
    } else {
      as_bstrins_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
    }
  }

  void storeValue(ValueOperand val, const Address& dest);
  void storeValue(ValueOperand val, const BaseIndex& dest);
  void storeValue(JSValueType type, Register reg, Address dest);
  void storeValue(JSValueType type, Register reg, BaseIndex dest);
  void storeValue(const Value& val, Address dest);
  void storeValue(const Value& val, BaseIndex dest);
  void storeValue(const Address& src, const Address& dest, Register temp) {
    loadPtr(src, temp);
    storePtr(temp, dest);
  }

  void storePrivateValue(Register src, const Address& dest) {
    storePtr(src, dest);
  }
  void storePrivateValue(ImmGCPtr imm, const Address& dest) {
    storePtr(imm, dest);
  }

  void loadValue(Address src, ValueOperand val);
  void loadValue(const BaseIndex& src, ValueOperand val);

  void loadUnalignedValue(const Address& src, ValueOperand dest) {
    loadValue(src, dest);
  }

  void tagValue(JSValueType type, Register payload, ValueOperand dest);

  void pushValue(ValueOperand val);
  void popValue(ValueOperand val);
  void pushValue(const Value& val) {
    if (val.isGCThing()) {
      ScratchRegisterScope scratch(asMasm());
      writeDataRelocation(val);
      movWithPatch(ImmWord(val.asRawBits()), scratch);
      push(scratch);
    } else {
      push(ImmWord(val.asRawBits()));
    }
  }
  void pushValue(JSValueType type, Register reg) {
    SecondScratchRegisterScope scratch2(asMasm());
    boxValue(type, reg, scratch2);
    push(scratch2);
  }
  void pushValue(const Address& addr);

  void handleFailureWithHandlerTail(void* handler);

  /////////////////////////////////////////////////////////////////
  // Common interface.
  /////////////////////////////////////////////////////////////////
 public:
  // The following functions are exposed for use in platform-shared code.

  inline void incrementInt32Value(const Address& addr);

  void move32(Imm32 imm, Register dest);
  void move32(Register src, Register dest);

  void movePtr(Register src, Register dest);
  void movePtr(ImmWord imm, Register dest);
  void movePtr(ImmPtr imm, Register dest);
  void movePtr(wasm::SymbolicAddress imm, Register dest);
  void movePtr(ImmGCPtr imm, Register dest);

  void load8SignExtend(const Address& address, Register dest);
  void load8SignExtend(const BaseIndex& src, Register dest);

  void load8ZeroExtend(const Address& address, Register dest);
  void load8ZeroExtend(const BaseIndex& src, Register dest);

  void load16SignExtend(const Address& address, Register dest);
  void load16SignExtend(const BaseIndex& src, Register dest);

  template <typename S>
  void load16UnalignedSignExtend(const S& src, Register dest) {
    load16SignExtend(src, dest);
  }

  void load16ZeroExtend(const Address& address, Register dest);
  void load16ZeroExtend(const BaseIndex& src, Register dest);

  template <typename S>
  void load16UnalignedZeroExtend(const S& src, Register dest) {
    load16ZeroExtend(src, dest);
  }

  void load32(const Address& address, Register dest);
  void load32(const BaseIndex& address, Register dest);
  void load32(AbsoluteAddress address, Register dest);
  void load32(wasm::SymbolicAddress address, Register dest);

  template <typename S>
  void load32Unaligned(const S& src, Register dest) {
    load32(src, dest);
  }

  void load64(const Address& address, Register64 dest) {
    loadPtr(address, dest.reg);
  }
  void load64(const BaseIndex& address, Register64 dest) {
    loadPtr(address, dest.reg);
  }

  template <typename S>
  void load64Unaligned(const S& src, Register64 dest) {
    load64(src, dest);
  }

  void loadPtr(const Address& address, Register dest);
  void loadPtr(const BaseIndex& src, Register dest);
  void loadPtr(AbsoluteAddress address, Register dest);
  void loadPtr(wasm::SymbolicAddress address, Register dest);

  void loadPrivate(const Address& address, Register dest);

  void store8(Register src, const Address& address);
  void store8(Imm32 imm, const Address& address);
  void store8(Register src, const BaseIndex& address);
  void store8(Imm32 imm, const BaseIndex& address);

  void store16(Register src, const Address& address);
  void store16(Imm32 imm, const Address& address);
  void store16(Register src, const BaseIndex& address);
  void store16(Imm32 imm, const BaseIndex& address);

  template <typename T>
  void store16Unaligned(Register src, const T& dest) {
    store16(src, dest);
  }

  void store32(Register src, AbsoluteAddress address);
  void store32(Register src, const Address& address);
  void store32(Register src, const BaseIndex& address);
  void store32(Imm32 src, const Address& address);
  void store32(Imm32 src, const BaseIndex& address);

  // NOTE: This will use second scratch on LOONGARCH64. Only ARM needs the
  // implementation without second scratch.
  void store32_NoSecondScratch(Imm32 src, const Address& address) {
    store32(src, address);
  }

  template <typename T>
  void store32Unaligned(Register src, const T& dest) {
    store32(src, dest);
  }

  void store64(Imm64 imm, Address address) {
    storePtr(ImmWord(imm.value), address);
  }
  void store64(Imm64 imm, const BaseIndex& address) {
    storePtr(ImmWord(imm.value), address);
  }

  void store64(Register64 src, Address address) { storePtr(src.reg, address); }
  void store64(Register64 src, const BaseIndex& address) {
    storePtr(src.reg, address);
  }

  template <typename T>
  void store64Unaligned(Register64 src, const T& dest) {
    store64(src, dest);
  }

  template <typename T>
  void storePtr(ImmWord imm, T address);
  template <typename T>
  void storePtr(ImmPtr imm, T address);
  template <typename T>
  void storePtr(ImmGCPtr imm, T address);
  void storePtr(Register src, const Address& address);
  void storePtr(Register src, const BaseIndex& address);
  void storePtr(Register src, AbsoluteAddress dest);

  void atomicEffectOp(int nbytes, AtomicOp op, Imm32 value,
                      const Address& address, Register valueTemp,
                      Register offsetTemp, Register maskTemp);
  void atomicEffectOp(int nbytes, AtomicOp op, Imm32 value,
                      const BaseIndex& address, Register valueTemp,
                      Register offsetTemp, Register maskTemp);
  void atomicEffectOp(int nbytes, AtomicOp op, Register value,
                      const Address& address, Register valueTemp,
                      Register offsetTemp, Register maskTemp);
  void atomicEffectOp(int nbytes, AtomicOp op, Register value,
                      const BaseIndex& address, Register valueTemp,
                      Register offsetTemp, Register maskTemp);
  void atomicEffectOp(int nbytes, AtomicOp op, Imm32 value,
                      const Address& address, Register flagTemp,
                      Register valueTemp, Register offsetTemp,
                      Register maskTemp) {
    atomicEffectOp(nbytes, op, value, address, valueTemp, offsetTemp,
                   maskTemp);
  }
  void atomicEffectOp(int nbytes, AtomicOp op, Imm32 value,
                      const BaseIndex& address, Register flagTemp,
                      Register valueTemp, Register offsetTemp,
                      Register maskTemp) {
    atomicEffectOp(nbytes, op, value, address, valueTemp, offsetTemp,
                   maskTemp);
  }
  void atomicEffectOp(int nbytes, AtomicOp op, Register value,
                      const Address& address, Register flagTemp,
                      Register valueTemp, Register offsetTemp,
                      Register maskTemp) {
    atomicEffectOp(nbytes, op, value, address, valueTemp, offsetTemp,
                   maskTemp);
  }
  void atomicEffectOp(int nbytes, AtomicOp op, Register value,
                      const BaseIndex& address, Register flagTemp,
                      Register valueTemp, Register offsetTemp,
                      Register maskTemp) {
    atomicEffectOp(nbytes, op, value, address, valueTemp, offsetTemp,
                   maskTemp);
  }

  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Imm32 value,
                     const Address& address, Register valueTemp,
                     Register offsetTemp, Register maskTemp, Register output);
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Imm32 value,
                     const BaseIndex& address, Register valueTemp,
                     Register offsetTemp, Register maskTemp, Register output);
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Register value,
                     const Address& address, Register valueTemp,
                     Register offsetTemp, Register maskTemp, Register output);
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Register value,
                     const BaseIndex& address, Register valueTemp,
                     Register offsetTemp, Register maskTemp, Register output);
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Imm32 value,
                     const Address& address, Register flagTemp,
                     Register valueTemp, Register offsetTemp,
                     Register maskTemp, Register output) {
    atomicFetchOp(nbytes, signExtend, op, value, address, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Imm32 value,
                     const BaseIndex& address, Register flagTemp,
                     Register valueTemp, Register offsetTemp,
                     Register maskTemp, Register output) {
    atomicFetchOp(nbytes, signExtend, op, value, address, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Register value,
                     const Address& address, Register flagTemp,
                     Register valueTemp, Register offsetTemp,
                     Register maskTemp, Register output) {
    atomicFetchOp(nbytes, signExtend, op, value, address, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  void atomicFetchOp(int nbytes, bool signExtend, AtomicOp op, Register value,
                     const BaseIndex& address, Register flagTemp,
                     Register valueTemp, Register offsetTemp,
                     Register maskTemp, Register output) {
    atomicFetchOp(nbytes, signExtend, op, value, address, valueTemp,
                  offsetTemp, maskTemp, output);
  }

  template <typename T, typename S>
  void atomicFetchAdd8SignExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, true, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAdd8ZeroExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, false, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAdd16SignExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, true, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAdd16ZeroExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, false, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAdd32(const S& value, const T& mem, Register flagTemp,
                        Register valueTemp, Register offsetTemp,
                        Register maskTemp, Register output) {
    atomicFetchOp(4, false, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicAdd8(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(1, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicAdd16(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(2, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicAdd32(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(4, AtomicFetchAddOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }

  template <typename T, typename S>
  void atomicFetchSub8SignExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, true, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchSub8ZeroExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, false, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchSub16SignExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, true, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchSub16ZeroExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, false, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchSub32(const S& value, const T& mem, Register flagTemp,
                        Register valueTemp, Register offsetTemp,
                        Register maskTemp, Register output) {
    atomicFetchOp(4, false, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicSub8(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(1, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicSub16(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(2, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicSub32(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(4, AtomicFetchSubOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }

  template <typename T, typename S>
  void atomicFetchAnd8SignExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, true, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAnd8ZeroExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, false, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAnd16SignExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, true, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAnd16ZeroExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, false, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchAnd32(const S& value, const T& mem, Register flagTemp,
                        Register valueTemp, Register offsetTemp,
                        Register maskTemp, Register output) {
    atomicFetchOp(4, false, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicAnd8(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(1, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicAnd16(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(2, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicAnd32(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(4, AtomicFetchAndOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }

  template <typename T, typename S>
  void atomicFetchOr8SignExtend(const S& value, const T& mem,
                                Register flagTemp, Register valueTemp,
                                Register offsetTemp, Register maskTemp,
                                Register output) {
    atomicFetchOp(1, true, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchOr8ZeroExtend(const S& value, const T& mem,
                                Register flagTemp, Register valueTemp,
                                Register offsetTemp, Register maskTemp,
                                Register output) {
    atomicFetchOp(1, false, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchOr16SignExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(2, true, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchOr16ZeroExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(2, false, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchOr32(const S& value, const T& mem, Register flagTemp,
                       Register valueTemp, Register offsetTemp,
                       Register maskTemp, Register output) {
    atomicFetchOp(4, false, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicOr8(const T& value, const S& mem, Register flagTemp,
                 Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(1, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicOr16(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(2, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicOr32(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(4, AtomicFetchOrOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }

  template <typename T, typename S>
  void atomicFetchXor8SignExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, true, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchXor8ZeroExtend(const S& value, const T& mem,
                                 Register flagTemp, Register valueTemp,
                                 Register offsetTemp, Register maskTemp,
                                 Register output) {
    atomicFetchOp(1, false, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchXor16SignExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, true, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchXor16ZeroExtend(const S& value, const T& mem,
                                  Register flagTemp, Register valueTemp,
                                  Register offsetTemp, Register maskTemp,
                                  Register output) {
    atomicFetchOp(2, false, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicFetchXor32(const S& value, const T& mem, Register flagTemp,
                        Register valueTemp, Register offsetTemp,
                        Register maskTemp, Register output) {
    atomicFetchOp(4, false, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                  offsetTemp, maskTemp, output);
  }
  template <typename T, typename S>
  void atomicXor8(const T& value, const S& mem, Register flagTemp,
                  Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(1, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicXor16(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(2, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }
  template <typename T, typename S>
  void atomicXor32(const T& value, const S& mem, Register flagTemp,
                   Register valueTemp, Register offsetTemp, Register maskTemp) {
    atomicEffectOp(4, AtomicFetchXorOp, value, mem, flagTemp, valueTemp,
                   offsetTemp, maskTemp);
  }

  void compareExchange(int nbytes, bool signExtend, const Address& address,
                       Register oldval, Register newval, Register valueTemp,
                       Register offsetTemp, Register maskTemp, Register output);
  void compareExchange(int nbytes, bool signExtend, const BaseIndex& address,
                       Register oldval, Register newval, Register valueTemp,
                       Register offsetTemp, Register maskTemp, Register output);
  void compareExchangeToTypedIntArray(Scalar::Type arrayType,
                                      const Address& mem, Register oldval,
                                      Register newval, Register temp,
                                      Register valueTemp, Register offsetTemp,
                                      Register maskTemp, AnyRegister output);
  void compareExchangeToTypedIntArray(Scalar::Type arrayType,
                                      const BaseIndex& mem, Register oldval,
                                      Register newval, Register temp,
                                      Register valueTemp, Register offsetTemp,
                                      Register maskTemp, AnyRegister output);

  void atomicExchange(int nbytes, bool signExtend, const Address& address,
                      Register value, Register valueTemp, Register offsetTemp,
                      Register maskTemp, Register output);
  void atomicExchange(int nbytes, bool signExtend, const BaseIndex& address,
                      Register value, Register valueTemp, Register offsetTemp,
                      Register maskTemp, Register output);
  void atomicExchangeToTypedIntArray(Scalar::Type arrayType,
                                     const Address& mem, Register value,
                                     Register temp, Register valueTemp,
                                     Register offsetTemp, Register maskTemp,
                                     AnyRegister output);
  void atomicExchangeToTypedIntArray(Scalar::Type arrayType,
                                     const BaseIndex& mem, Register value,
                                     Register temp, Register valueTemp,
                                     Register offsetTemp, Register maskTemp,
                                     AnyRegister output);

  void moveDouble(FloatRegister src, FloatRegister dest) {
    as_fmov_d(dest, src);
  }

  void zeroDouble(FloatRegister reg) { moveToDouble(zero, reg); }

  void cmp64Set(Assembler::Condition cond, Register lhs, Imm32 rhs,
                Register dest) {
    ma_cmp_set(dest, lhs, rhs, cond);
  }

  void convertUInt64ToDouble(Register src, FloatRegister dest);
  static bool convertUInt64ToDoubleNeedsTemp();
  void convertUInt64ToDouble(Register64 src, FloatRegister dest, Register temp);

  void breakpoint(uint32_t value = 0);

  void checkStackAlignment() {
#ifdef DEBUG
    Label aligned;
    ScratchRegisterScope scratch(asMasm());
    as_andi(scratch, sp, ABIStackAlignment - 1);
    ma_b(scratch, zero, &aligned, Equal, ShortJump);
    breakpoint();
    bind(&aligned);
#endif
  };

  static void calculateAlignedStackPointer(void** stackPointer);

  void loadWasmGlobalPtr(uint32_t globalDataOffset, Register dest) {
    loadPtr(Address(GlobalReg, globalDataOffset - WasmGlobalRegBias), dest);
  }
  void loadWasmPinnedRegsFromTls() {
    loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, memoryBase)), HeapReg);
    loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, globalData)),
            GlobalReg);
    ma_add_d(GlobalReg, GlobalReg, Imm32(WasmGlobalRegBias));
  }

  void cmpPtrSet(Assembler::Condition cond, Address lhs, ImmPtr rhs,
                 Register dest);
  void cmpPtrSet(Assembler::Condition cond, Register lhs, Address rhs,
                 Register dest);
  void cmpPtrSet(Assembler::Condition cond, Address lhs, Register rhs,
                 Register dest);

  void cmp32Set(Assembler::Condition cond, Register lhs, Address rhs,
                Register dest);

 protected:
  bool buildOOLFakeExitFrame(void* fakeReturnAddr);

  void wasmLoadI64Impl(const wasm::MemoryAccessDesc& access,
                       Register memoryBase, Register ptr, Register ptrScratch,
                       Register64 output, Register tmp);
  void wasmStoreI64Impl(const wasm::MemoryAccessDesc& access, Register64 value,
                        Register memoryBase, Register ptr, Register ptrScratch,
                        Register tmp);

 public:
  CodeOffset labelForPatch() {
    return CodeOffset(nextOffset().getOffset());
  }

  void lea(Operand addr, Register dest) {
    ma_add_d(dest, addr.baseReg(), Imm32(addr.disp()));
  }

  void abiret() { as_jirl(zero, ra, BOffImm16(0)); }

  void moveFloat32(FloatRegister src, FloatRegister dest) {
    as_fmov_s(dest, src);
  }

  // Instrumentation for entering and leaving the profiler.
  void profilerEnterFrame(Register framePtr, Register scratch);
  void profilerExitFrame();
};

typedef MacroAssemblerLOONGARCH64Compat MacroAssemblerSpecific;

}  // namespace jit
}  // namespace js

#endif /* jit_loongarch64_MacroAssembler_loongarch64_h */
