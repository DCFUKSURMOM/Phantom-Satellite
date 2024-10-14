/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_generator_h
#define wasm_generator_h

#include "jit/MacroAssembler.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmModule.h"

namespace js {
namespace wasm {

class FunctionGenerator;

// The ModuleGeneratorData holds all the state shared between the
// ModuleGenerator thread and background compile threads. The background
// threads are given a read-only view of the ModuleGeneratorData and the
// ModuleGenerator is careful to initialize, and never subsequently mutate,
// any given datum before being read by a background thread. In particular,
// once created, the Vectors are never resized.

struct ModuleGeneratorData
{
    ModuleKind                kind;
    MemoryUsage               memoryUsage;
    mozilla::Atomic<uint32_t> minMemoryLength;
    Maybe<uint32_t>           maxMemoryLength;

    SigWithIdVector           sigs;
    SigWithIdPtrVector        funcSigs;
    Uint32Vector              funcImportGlobalDataOffsets;
    GlobalDescVector          globals;
    TableDescVector           tables;
    Uint32Vector              asmJSSigToTableIndex;

    explicit ModuleGeneratorData(ModuleKind kind = ModuleKind::Wasm)
      : kind(kind),
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0)
    {}

    bool isAsmJS() const {
        return kind == ModuleKind::AsmJS;
    }
    bool funcIsImport(uint32_t funcIndex) const {
        return funcIndex < funcImportGlobalDataOffsets.length();
    }
};

typedef UniquePtr<ModuleGeneratorData> UniqueModuleGeneratorData;

// A ModuleGenerator encapsulates the creation of a wasm module. During the
// lifetime of a ModuleGenerator, a sequence of FunctionGenerators are created
// and destroyed to compile the individual function bodies. After generating all
// functions, ModuleGenerator::finish() must be called to complete the
// compilation and extract the resulting wasm module.

class MOZ_STACK_CLASS ModuleGenerator
{
    typedef HashSet<uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> Uint32Set;
    typedef Vector<IonCompileTask, 0, SystemAllocPolicy> IonCompileTaskVector;
    typedef Vector<IonCompileTask*, 0, SystemAllocPolicy> IonCompileTaskPtrVector;
    typedef EnumeratedArray<Trap, Trap::Limit, ProfilingOffsets> TrapExitOffsetArray;

    // Constant parameters
    bool                            alwaysBaseline_;

    // Data that is moved into the result of finish()
    Assumptions                     assumptions_;
    LinkData                        linkData_;
    MutableMetadata                 metadata_;
    ExportVector                    exports_;
    ImportVector                    imports_;
    DataSegmentVector               dataSegments_;
    ElemSegmentVector               elemSegments_;

    // Data scoped to the ModuleGenerator's lifetime
    UniqueModuleGeneratorData       shared_;
    uint32_t                        numSigs_;
    uint32_t                        numTables_;
    LifoAlloc                       lifo_;
    jit::JitContext                 jcx_;
    jit::TempAllocator              masmAlloc_;
    jit::MacroAssembler             masm_;
    Uint32Vector                    funcToCodeRange_;
    Uint32Set                       exportedFuncs_;
    uint32_t                        lastPatchedCallsite_;
    uint32_t                        startOfUnpatchedCallsites_;

    // Parallel compilation
    bool                            parallel_;
    uint32_t                        outstanding_;
    IonCompileTaskVector            tasks_;
    IonCompileTaskPtrVector         freeTasks_;

    // Assertions
    DebugOnly<FunctionGenerator*>   activeFuncDef_;
    DebugOnly<bool>                 startedFuncDefs_;
    DebugOnly<bool>                 finishedFuncDefs_;
    DebugOnly<uint32_t>             numFinishedFuncDefs_;

    bool funcIsCompiled(uint32_t funcIndex) const;
    const CodeRange& funcCodeRange(uint32_t funcIndex) const;
    [[nodiscard]] bool patchCallSites(TrapExitOffsetArray* maybeTrapExits = nullptr);
    [[nodiscard]] bool patchFarJumps(const TrapExitOffsetArray& trapExits);
    [[nodiscard]] bool finishTask(IonCompileTask* task);
    [[nodiscard]] bool finishOutstandingTask();
    [[nodiscard]] bool finishFuncExports();
    [[nodiscard]] bool finishCodegen();
    [[nodiscard]] bool finishLinkData(Bytes& code);
    [[nodiscard]] bool addFuncImport(const Sig& sig, uint32_t globalDataOffset);
    [[nodiscard]] bool allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOff);
    [[nodiscard]] bool allocateGlobal(GlobalDesc* global);

  public:
    explicit ModuleGenerator(ImportVector&& imports);
    ~ModuleGenerator();

    [[nodiscard]] bool init(UniqueModuleGeneratorData shared, const CompileArgs& args,
                           Metadata* maybeAsmJSMetadata = nullptr);

    bool isAsmJS() const { return metadata_->kind == ModuleKind::AsmJS; }
    jit::MacroAssembler& masm() { return masm_; }

    // Memory:
    bool usesMemory() const { return UsesMemory(shared_->memoryUsage); }
    uint32_t minMemoryLength() const { return shared_->minMemoryLength; }

    // Tables:
    uint32_t numTables() const { return numTables_; }
    const TableDescVector& tables() const { return shared_->tables; }

    // Signatures:
    uint32_t numSigs() const { return numSigs_; }
    const SigWithId& sig(uint32_t sigIndex) const;
    const SigWithId& funcSig(uint32_t funcIndex) const;

    // Globals:
    const GlobalDescVector& globals() const { return shared_->globals; }

    // Functions declarations:
    uint32_t numFuncImports() const;
    uint32_t numFuncDefs() const;
    uint32_t numFuncs() const;

    // Exports:
    [[nodiscard]] bool addFuncExport(UniqueChars fieldName, uint32_t funcIndex);
    [[nodiscard]] bool addTableExport(UniqueChars fieldName);
    [[nodiscard]] bool addMemoryExport(UniqueChars fieldName);
    [[nodiscard]] bool addGlobalExport(UniqueChars fieldName, uint32_t globalIndex);

    // Function definitions:
    [[nodiscard]] bool startFuncDefs();
    [[nodiscard]] bool startFuncDef(uint32_t lineOrBytecode, FunctionGenerator* fg);
    [[nodiscard]] bool finishFuncDef(uint32_t funcIndex, FunctionGenerator* fg);
    [[nodiscard]] bool finishFuncDefs();

    // Start function:
    bool setStartFunction(uint32_t funcIndex);

    // Segments:
    void setDataSegments(DataSegmentVector&& segments);
    [[nodiscard]] bool addElemSegment(InitExpr offset, Uint32Vector&& elemFuncIndices);

    // Function names:
    void setFuncNames(NameInBytecodeVector&& funcNames);

    // asm.js lazy initialization:
    void initSig(uint32_t sigIndex, Sig&& sig);
    void initFuncSig(uint32_t funcIndex, uint32_t sigIndex);
    [[nodiscard]] bool initImport(uint32_t funcIndex, uint32_t sigIndex);
    [[nodiscard]] bool initSigTableLength(uint32_t sigIndex, uint32_t length);
    [[nodiscard]] bool initSigTableElems(uint32_t sigIndex, Uint32Vector&& elemFuncIndices);
    void initMemoryUsage(MemoryUsage memoryUsage);
    void bumpMinMemoryLength(uint32_t newMinMemoryLength);
    [[nodiscard]] bool addGlobal(ValType type, bool isConst, uint32_t* index);

    // Finish compilation, provided the list of imports and source bytecode.
    // Both these Vectors may be empty (viz., b/c asm.js does different things
    // for imports and source).
    SharedModule finish(const ShareableBytes& bytecode);
};

// A FunctionGenerator encapsulates the generation of a single function body.
// ModuleGenerator::startFunc must be called after construction and before doing
// anything else. After the body is complete, ModuleGenerator::finishFunc must
// be called before the FunctionGenerator is destroyed and the next function is
// started.

class MOZ_STACK_CLASS FunctionGenerator
{
    friend class ModuleGenerator;

    ModuleGenerator* m_;
    IonCompileTask*  task_;
    bool             usesSimd_;
    bool             usesAtomics_;

    // Data created during function generation, then handed over to the
    // FuncBytes in ModuleGenerator::finishFunc().
    Bytes            bytes_;
    Uint32Vector     callSiteLineNums_;

    uint32_t lineOrBytecode_;

  public:
    FunctionGenerator()
      : m_(nullptr), task_(nullptr), usesSimd_(false), usesAtomics_(false), lineOrBytecode_(0)
    {}

    bool usesSimd() const {
        return usesSimd_;
    }
    void setUsesSimd() {
        usesSimd_ = true;
    }

    bool usesAtomics() const {
        return usesAtomics_;
    }
    void setUsesAtomics() {
        usesAtomics_ = true;
    }

    Bytes& bytes() {
        return bytes_;
    }
    [[nodiscard]] bool addCallSiteLineNum(uint32_t lineno) {
        return callSiteLineNums_.append(lineno);
    }
};

} // namespace wasm
} // namespace js

#endif // wasm_generator_h
