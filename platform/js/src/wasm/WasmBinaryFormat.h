/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2016 Mozilla Foundation
 * Copyright 2023 Moonchild Productions
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_binary_format_h
#define wasm_binary_format_h

#include "wasm/WasmTypes.h"

namespace js {
namespace wasm {

// The Encoder class appends bytes to the Bytes object it is given during
// construction. The client is responsible for the Bytes's lifetime and must
// keep the Bytes alive as long as the Encoder is used.

class Encoder
{
    Bytes& bytes_;

    template <class T>
    [[nodiscard]] bool write(const T& v) {
        return bytes_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    }

    template <typename UInt>
    [[nodiscard]] bool writeVarU(UInt i) {
        do {
            uint8_t byte = i & 0x7f;
            i >>= 7;
            if (i != 0)
                byte |= 0x80;
            if (!bytes_.append(byte))
                return false;
        } while (i != 0);
        return true;
    }

    template <typename SInt>
    [[nodiscard]] bool writeVarS(SInt i) {
        bool done;
        do {
            uint8_t byte = i & 0x7f;
            i >>= 7;
            done = ((i == 0) && !(byte & 0x40)) || ((i == -1) && (byte & 0x40));
            if (!done)
                byte |= 0x80;
            if (!bytes_.append(byte))
                return false;
        } while (!done);
        return true;
    }

    void patchVarU32(size_t offset, uint32_t patchBits, uint32_t assertBits) {
        do {
            uint8_t assertByte = assertBits & 0x7f;
            uint8_t patchByte = patchBits & 0x7f;
            assertBits >>= 7;
            patchBits >>= 7;
            if (assertBits != 0) {
                assertByte |= 0x80;
                patchByte |= 0x80;
            }
            MOZ_ASSERT(assertByte == bytes_[offset]);
            bytes_[offset] = patchByte;
            offset++;
        } while(assertBits != 0);
    }

    void patchFixedU7(size_t offset, uint8_t patchBits, uint8_t assertBits) {
        MOZ_ASSERT(patchBits <= uint8_t(INT8_MAX));
        patchFixedU8(offset, patchBits, assertBits);
    }

    void patchFixedU8(size_t offset, uint8_t patchBits, uint8_t assertBits) {
        MOZ_ASSERT(bytes_[offset] == assertBits);
        bytes_[offset] = patchBits;
    }

    uint32_t varU32ByteLength(size_t offset) const {
        size_t start = offset;
        while (bytes_[offset] & 0x80)
            offset++;
        return offset - start + 1;
    }

  public:
    explicit Encoder(Bytes& bytes)
      : bytes_(bytes)
    {
        MOZ_ASSERT(empty());
    }

    size_t currentOffset() const { return bytes_.length(); }
    bool empty() const { return currentOffset() == 0; }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    [[nodiscard]] bool writeFixedU7(uint8_t i) {
        MOZ_ASSERT(i <= uint8_t(INT8_MAX));
        return writeFixedU8(i);
    }
    [[nodiscard]] bool writeFixedU8(uint8_t i) {
        return write<uint8_t>(i);
    }
    [[nodiscard]] bool writeFixedU32(uint32_t i) {
        return write<uint32_t>(i);
    }
    [[nodiscard]] bool writeFixedF32(RawF32 f) {
        return write<uint32_t>(f.bits());
    }
    [[nodiscard]] bool writeFixedF64(RawF64 d) {
        return write<uint64_t>(d.bits());
    }

    // Variable-length encodings that all use LEB128.

    [[nodiscard]] bool writeVarU32(uint32_t i) {
        return writeVarU<uint32_t>(i);
    }
    [[nodiscard]] bool writeVarS32(int32_t i) {
        return writeVarS<int32_t>(i);
    }
    [[nodiscard]] bool writeVarU64(uint64_t i) {
        return writeVarU<uint64_t>(i);
    }
    [[nodiscard]] bool writeVarS64(int64_t i) {
        return writeVarS<int64_t>(i);
    }
    [[nodiscard]] bool writeValType(ValType type) {
        static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
        MOZ_ASSERT(size_t(type) < size_t(TypeCode::Limit));
        return writeFixedU8(uint8_t(type));
    }
    [[nodiscard]] bool writeBlockType(ExprType type) {
        static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
        MOZ_ASSERT(size_t(type) < size_t(TypeCode::Limit));
        return writeFixedU8(uint8_t(type));
    }
    [[nodiscard]] bool writeOp(Op op) {
        static_assert(size_t(Op::Limit) <= 2 * UINT8_MAX, "fits");
        MOZ_ASSERT(size_t(op) < size_t(Op::Limit));
        if (size_t(op) < UINT8_MAX)
            return writeFixedU8(uint8_t(op));
        return writeFixedU8(UINT8_MAX) &&
               writeFixedU8(size_t(op) - UINT8_MAX);
    }

    // Fixed-length encodings that allow back-patching.

    [[nodiscard]] bool writePatchableFixedU7(size_t* offset) {
        *offset = bytes_.length();
        return writeFixedU8(UINT8_MAX);
    }
    void patchFixedU7(size_t offset, uint8_t patchBits) {
        return patchFixedU7(offset, patchBits, UINT8_MAX);
    }

    // Variable-length encodings that allow back-patching.

    [[nodiscard]] bool writePatchableVarU32(size_t* offset) {
        *offset = bytes_.length();
        return writeVarU32(UINT32_MAX);
    }
    void patchVarU32(size_t offset, uint32_t patchBits) {
        return patchVarU32(offset, patchBits, UINT32_MAX);
    }

    // Byte ranges start with an LEB128 length followed by an arbitrary sequence
    // of bytes. When used for strings, bytes are to be interpreted as utf8.

    [[nodiscard]] bool writeBytes(const void* bytes, uint32_t numBytes) {
        return writeVarU32(numBytes) &&
               bytes_.append(reinterpret_cast<const uint8_t*>(bytes), numBytes);
    }

    // A "section" is a contiguous range of bytes that stores its own size so
    // that it may be trivially skipped without examining the contents. Sections
    // require backpatching since the size of the section is only known at the
    // end while the size's varU32 must be stored at the beginning. Immediately
    // after the section length is the string id of the section.

    [[nodiscard]] bool startSection(SectionId id, size_t* offset) {
        MOZ_ASSERT(id != SectionId::UserDefined); // not supported yet

        return writeVarU32(uint32_t(id)) &&
               writePatchableVarU32(offset);
    }
    void finishSection(size_t offset) {
        return patchVarU32(offset, bytes_.length() - offset - varU32ByteLength(offset));
    }
};

// The Decoder class decodes the bytes in the range it is given during
// construction. The client is responsible for keeping the byte range alive as
// long as the Decoder is used.

class Decoder
{
    const uint8_t* const beg_;
    const uint8_t* const end_;
    const uint8_t* cur_;
    UniqueChars* error_;

    template <class T>
    [[nodiscard]] bool read(T* out) {
        if (bytesRemain() < sizeof(T))
            return false;
        memcpy((void*)out, cur_, sizeof(T));
        cur_ += sizeof(T);
        return true;
    }

    template <class T>
    T uncheckedRead() {
        MOZ_ASSERT(bytesRemain() >= sizeof(T));
        T ret;
        memcpy(&ret, cur_, sizeof(T));
        cur_ += sizeof(T);
        return ret;
    }

    template <class T>
    void uncheckedRead(T* ret) {
        MOZ_ASSERT(bytesRemain() >= sizeof(T));
        memcpy(ret, cur_, sizeof(T));
        cur_ += sizeof(T);
    }

    template <typename UInt>
    [[nodiscard]] bool readVarU(UInt* out) {
        const unsigned numBits = sizeof(UInt) * CHAR_BIT;
        const unsigned remainderBits = numBits % 7;
        const unsigned numBitsInSevens = numBits - remainderBits;
        UInt u = 0;
        uint8_t byte;
        UInt shift = 0;
        do {
            if (!readFixedU8(&byte))
                return false;
            if (!(byte & 0x80)) {
                *out = u | UInt(byte) << shift;
                return true;
            }
            u |= UInt(byte & 0x7F) << shift;
            shift += 7;
        } while (shift != numBitsInSevens);
        if (!readFixedU8(&byte) || (byte & (unsigned(-1) << remainderBits)))
            return false;
        *out = u | (UInt(byte) << numBitsInSevens);
        return true;
    }

    template <typename SInt>
    [[nodiscard]] bool readVarS(SInt* out) {
        const unsigned numBits = sizeof(SInt) * CHAR_BIT;
        const unsigned remainderBits = numBits % 7;
        const unsigned numBitsInSevens = numBits - remainderBits;
        SInt s = 0;
        uint8_t byte;
        unsigned shift = 0;
        do {
            if (!readFixedU8(&byte))
                return false;
            s |= SInt(byte & 0x7f) << shift;
            shift += 7;
            if (!(byte & 0x80)) {
                if (byte & 0x40)
                    s |= SInt(-1) << shift;
                *out = s;
                return true;
            }
        } while (shift < numBitsInSevens);
        if (!remainderBits || !readFixedU8(&byte) || (byte & 0x80))
            return false;
        uint8_t mask = 0x7f & (uint8_t(-1) << remainderBits);
        if ((byte & mask) != ((byte & (1 << (remainderBits - 1))) ? mask : 0))
            return false;
        *out = s | SInt(byte) << shift;
        return true;
    }

  public:
    Decoder(const uint8_t* begin, const uint8_t* end, UniqueChars* error)
      : beg_(begin),
        end_(end),
        cur_(begin),
        error_(error)
    {
        MOZ_ASSERT(begin <= end);
    }
    explicit Decoder(const Bytes& bytes, UniqueChars* error = nullptr)
      : beg_(bytes.begin()),
        end_(bytes.end()),
        cur_(bytes.begin()),
        error_(error)
    {}

    bool fail(const char* msg, ...) MOZ_FORMAT_PRINTF(2, 3);
    bool fail(UniqueChars msg);
    void clearError() {
        if (error_)
            error_->reset();
    }

    bool done() const {
        MOZ_ASSERT(cur_ <= end_);
        return cur_ == end_;
    }

    size_t bytesRemain() const {
        MOZ_ASSERT(end_ >= cur_);
        return size_t(end_ - cur_);
    }
    // pos must be a value previously returned from currentPosition.
    void rollbackPosition(const uint8_t* pos) {
        cur_ = pos;
    }
    const uint8_t* currentPosition() const {
        return cur_;
    }
    size_t currentOffset() const {
        return cur_ - beg_;
    }
    const uint8_t* begin() const {
        return beg_;
    }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    [[nodiscard]] bool readFixedU8(uint8_t* i) {
        return read<uint8_t>(i);
    }
    [[nodiscard]] bool readFixedU32(uint32_t* u) {
        return read<uint32_t>(u);
    }
    [[nodiscard]] bool readFixedF32(RawF32* f) {
        uint32_t u;
        if (!read<uint32_t>(&u))
            return false;
        *f = RawF32::fromBits(u);
        return true;
    }
    [[nodiscard]] bool readFixedF64(RawF64* d) {
        uint64_t u;
        if (!read<uint64_t>(&u))
            return false;
        *d = RawF64::fromBits(u);
        return true;
    }

    // Variable-length encodings that all use LEB128.

    [[nodiscard]] bool readVarU32(uint32_t* out) {
        return readVarU<uint32_t>(out);
    }
    [[nodiscard]] bool readVarS32(int32_t* out) {
        return readVarS<int32_t>(out);
    }
    [[nodiscard]] bool readVarU64(uint64_t* out) {
        return readVarU<uint64_t>(out);
    }
    [[nodiscard]] bool readVarS64(int64_t* out) {
        return readVarS<int64_t>(out);
    }
    [[nodiscard]] bool readValType(uint8_t* type) {
        static_assert(uint8_t(TypeCode::Limit) <= UINT8_MAX, "fits");
        return readFixedU8(type);
    }
    [[nodiscard]] bool readBlockType(uint8_t* type) {
        static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
        return readFixedU8(type);
    }
    [[nodiscard]] bool readOp(uint16_t* op) {
        static_assert(size_t(Op::Limit) <= 2 * UINT8_MAX, "fits");
        uint8_t u8;
        if (!readFixedU8(&u8))
            return false;
        if (MOZ_LIKELY(u8 != UINT8_MAX)) {
            *op = u8;
            return true;
        }
        if (!readFixedU8(&u8))
            return false;
        *op = uint16_t(u8) + UINT8_MAX;
        return true;
    }

    // See writeBytes comment.

    [[nodiscard]] bool readBytes(uint32_t numBytes, const uint8_t** bytes = nullptr) {
        if (bytes)
            *bytes = cur_;
        if (bytesRemain() < numBytes)
            return false;
        cur_ += numBytes;
        return true;
    }

    // See "section" description in Encoder.

    static const uint32_t NotStarted = UINT32_MAX;

    [[nodiscard]] bool startSection(SectionId id,
                                   uint32_t* startOffset,
                                   uint32_t* size,
                                   const char* sectionName)
    {
        const uint8_t* const before = cur_;
        const uint8_t* beforeId = before;
        uint32_t idValue;
        if (!readVarU32(&idValue))
            goto backup;
        while (idValue != uint32_t(id)) {
            if (idValue != uint32_t(SectionId::UserDefined))
                goto backup;
            // Rewind to the section id since skipUserDefinedSection expects it.
            cur_ = beforeId;
            if (!skipUserDefinedSection())
                return false;
            beforeId = cur_;
            if (!readVarU32(&idValue))
                goto backup;
        }
        if (!readVarU32(size))
            goto fail;
        if (bytesRemain() < *size)
            goto fail;
        *startOffset = cur_ - beg_;
        return true;
      backup:
        cur_ = before;
        *startOffset = NotStarted;
        return true;
      fail:
        return fail("failed to start %s section", sectionName);
    }
    [[nodiscard]] bool finishSection(uint32_t startOffset, uint32_t size,
                                    const char* sectionName)
    {
        if (size != (cur_ - beg_) - startOffset)
            return fail("byte size mismatch in %s section", sectionName);
        return true;
    }

    // "User sections" do not cause validation errors unless the error is in
    // the user-defined section header itself.

    [[nodiscard]] bool startUserDefinedSection(const char* expectedId,
                                              size_t expectedIdSize,
                                              uint32_t* sectionStart,
                                              uint32_t* sectionSize)
    {
        const uint8_t* const before = cur_;
        while (true) {
            if (!startSection(SectionId::UserDefined, sectionStart, sectionSize, "user-defined"))
                return false;
            if (*sectionStart == NotStarted) {
                cur_ = before;
                return true;
            }
            uint32_t idSize;
            if (!readVarU32(&idSize))
                goto fail;
            if (idSize > bytesRemain() || currentOffset() + idSize > *sectionStart + *sectionSize)
                goto fail;
            if (expectedId && (expectedIdSize != idSize || !!memcmp(cur_, expectedId, idSize))) {
                finishUserDefinedSection(*sectionStart, *sectionSize);
                continue;
            }
            cur_ += idSize;
            return true;
        }
        MOZ_CRASH("unreachable");
      fail:
        return fail("failed to start user-defined section");
    }
    template <size_t IdSizeWith0>
    [[nodiscard]] bool startUserDefinedSection(const char (&id)[IdSizeWith0],
                                              uint32_t* sectionStart,
                                              uint32_t* sectionSize)
    {
        MOZ_ASSERT(id[IdSizeWith0 - 1] == '\0');
        return startUserDefinedSection(id, IdSizeWith0 - 1, sectionStart, sectionSize);
    }
    void finishUserDefinedSection(uint32_t sectionStart, uint32_t sectionSize) {
        MOZ_ASSERT(cur_ >= beg_);
        MOZ_ASSERT(cur_ <= end_);
        cur_ = (beg_ + sectionStart) + sectionSize;
        MOZ_ASSERT(cur_ <= end_);
        clearError();
    }
    [[nodiscard]] bool skipUserDefinedSection() {
        uint32_t sectionStart, sectionSize;
        if (!startUserDefinedSection(nullptr, 0, &sectionStart, &sectionSize))
            return false;
        if (sectionStart == NotStarted)
            return fail("expected user-defined section");
        finishUserDefinedSection(sectionStart, sectionSize);
        return true;
    }

    // The infallible "unchecked" decoding functions can be used when we are
    // sure that the bytes are well-formed (by construction or due to previous
    // validation).

    uint8_t uncheckedReadFixedU8() {
        return uncheckedRead<uint8_t>();
    }
    uint32_t uncheckedReadFixedU32() {
        return uncheckedRead<uint32_t>();
    }
    RawF32 uncheckedReadFixedF32() {
        return RawF32::fromBits(uncheckedRead<uint32_t>());
    }
    RawF64 uncheckedReadFixedF64() {
        return RawF64::fromBits(uncheckedRead<uint64_t>());
    }
    template <typename UInt>
    UInt uncheckedReadVarU() {
        static const unsigned numBits = sizeof(UInt) * CHAR_BIT;
        static const unsigned remainderBits = numBits % 7;
        static const unsigned numBitsInSevens = numBits - remainderBits;
        UInt decoded = 0;
        uint32_t shift = 0;
        do {
            uint8_t byte = *cur_++;
            if (!(byte & 0x80))
                return decoded | (UInt(byte) << shift);
            decoded |= UInt(byte & 0x7f) << shift;
            shift += 7;
        } while (shift != numBitsInSevens);
        uint8_t byte = *cur_++;
        MOZ_ASSERT(!(byte & 0xf0));
        return decoded | (UInt(byte) << numBitsInSevens);
    }
    uint32_t uncheckedReadVarU32() {
        return uncheckedReadVarU<uint32_t>();
    }
    int32_t uncheckedReadVarS32() {
        int32_t i32 = 0;
        MOZ_ALWAYS_TRUE(readVarS32(&i32));
        return i32;
    }
    uint64_t uncheckedReadVarU64() {
        return uncheckedReadVarU<uint64_t>();
    }
    int64_t uncheckedReadVarS64() {
        int64_t i64 = 0;
        MOZ_ALWAYS_TRUE(readVarS64(&i64));
        return i64;
    }
    ValType uncheckedReadValType() {
        return (ValType)uncheckedReadFixedU8();
    }
    Op uncheckedReadOp() {
        static_assert(size_t(Op::Limit) <= 2 * UINT8_MAX, "fits");
        uint8_t u8 = uncheckedReadFixedU8();
        return u8 != UINT8_MAX
               ? Op(u8)
               : Op(uncheckedReadFixedU8() + UINT8_MAX);
    }
};

// Reusable macro encoding/decoding functions reused by both the two
// encoders (AsmJS/WasmTextToBinary) and all the decoders
// (WasmCompile/WasmIonCompile/WasmBaselineCompile/WasmBinaryToText).

// Misc helpers.

UniqueChars
DecodeName(Decoder& d);

[[nodiscard]] bool
DecodeTableLimits(Decoder& d, TableDescVector* tables);

[[nodiscard]] bool
GlobalIsJSCompatible(Decoder& d, ValType type, bool isMutable);

[[nodiscard]] bool
EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

[[nodiscard]] bool
DecodeLocalEntries(Decoder& d, ModuleKind kind, ValTypeVector* locals);

[[nodiscard]] bool
DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable);

[[nodiscard]] bool
DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                            InitExpr* init);

[[nodiscard]] bool
DecodeLimits(Decoder& d, Limits* limits);

[[nodiscard]] bool
DecodeMemoryLimits(Decoder& d, bool hasMemory, Limits* memory);

// Section macros.

[[nodiscard]] bool
DecodePreamble(Decoder& d);

[[nodiscard]] bool
DecodeTypeSection(Decoder& d, SigWithIdVector* sigs);

[[nodiscard]] bool
DecodeImportSection(Decoder& d, const SigWithIdVector& sigs, Uint32Vector* funcSigIndices,
                    GlobalDescVector* globals, TableDescVector* tables, Maybe<Limits>* memory,
                    ImportVector* imports);

[[nodiscard]] bool
DecodeFunctionSection(Decoder& d, const SigWithIdVector& sigs, size_t numImportedFunc,
                      Uint32Vector* funcSigIndexes);

[[nodiscard]] bool
DecodeUnknownSections(Decoder& d);

[[nodiscard]] bool
DecodeDataSection(Decoder& d, bool usesMemory, uint32_t minMemoryByteLength,
                  const GlobalDescVector& globals, DataSegmentVector* segments);

[[nodiscard]] bool
DecodeMemorySection(Decoder& d, bool hasMemory, Limits* memory, bool* present);

} // namespace wasm
} // namespace js

#endif // wasm_binary_format_h
