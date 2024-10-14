/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS bytecode generation. */

#ifndef frontend_BytecodeEmitter_h
#define frontend_BytecodeEmitter_h

#include "jscntxt.h"
#include "jsiter.h"
#include "jsopcode.h"
#include "jsscript.h"

#include "ds/InlineTable.h"
#include "frontend/DestructuringFlavor.h"
#include "frontend/JumpList.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"
#include "frontend/SourceNotes.h"
#include "frontend/ValueUsage.h"
#include "vm/Interpreter.h"

class OptionalEmitter;

namespace js {
namespace frontend {

class FullParseHandler;
class ObjectBox;
class ParseNode;
template <typename ParseHandler> class Parser;
class SharedContext;
class TokenStream;

class CGConstList {
    Rooted<ValueVector> vector;
  public:
    explicit CGConstList(ExclusiveContext* cx)
      : vector(cx, ValueVector(cx))
    { }
    [[nodiscard]] bool append(const Value& v) {
        return vector.append(v);
    }
    size_t length() const { return vector.length(); }
    void finish(ConstArray* array);
};

struct CGObjectList {
    uint32_t            length;     /* number of emitted so far objects */
    ObjectBox*          lastbox;    /* last emitted object */

    CGObjectList() : length(0), lastbox(nullptr) {}

    unsigned add(ObjectBox* objbox);
    unsigned indexOf(JSObject* obj);
    void finish(ObjectArray* array);
    ObjectBox* find(uint32_t index);
};

struct MOZ_STACK_CLASS CGScopeList {
    Rooted<GCVector<Scope*>> vector;

    explicit CGScopeList(ExclusiveContext* cx)
      : vector(cx, GCVector<Scope*>(cx))
    { }

    bool append(Scope* scope) { return vector.append(scope); }
    uint32_t length() const { return vector.length(); }
    void finish(ScopeArray* array);
};

struct CGTryNoteList {
    Vector<JSTryNote> list;
    explicit CGTryNoteList(ExclusiveContext* cx) : list(cx) {}

    [[nodiscard]] bool append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end);
    size_t length() const { return list.length(); }
    void finish(TryNoteArray* array);
};

struct CGScopeNote : public ScopeNote
{
    // The end offset. Used to compute the length; may need adjusting first if
    // in the prologue.
    uint32_t end;

    // Is the start offset in the prologue?
    bool startInPrologue;

    // Is the end offset in the prologue?
    bool endInPrologue;
};

struct CGScopeNoteList {
    Vector<CGScopeNote> list;
    explicit CGScopeNoteList(ExclusiveContext* cx) : list(cx) {}

    [[nodiscard]] bool append(uint32_t scopeIndex, uint32_t offset, bool inPrologue,
                             uint32_t parent);
    void recordEnd(uint32_t index, uint32_t offset, bool inPrologue);
    size_t length() const { return list.length(); }
    void finish(ScopeNoteArray* array, uint32_t prologueLength);
};

struct CGYieldAndAwaitOffsetList {
    Vector<uint32_t> list;
    uint32_t numYields;
    uint32_t numAwaits;
    explicit CGYieldAndAwaitOffsetList(ExclusiveContext* cx) : list(cx), numYields(0), numAwaits(0) {}

    [[nodiscard]] bool append(uint32_t offset) { return list.append(offset); }
    size_t length() const { return list.length(); }
    void finish(YieldAndAwaitOffsetArray& array, uint32_t prologueLength);
};

static size_t MaxBytecodeLength = INT32_MAX;
static size_t MaxSrcNotesLength = INT32_MAX;

// Have a few inline elements to avoid heap allocation for tiny sequences.
typedef Vector<jsbytecode, 256> BytecodeVector;
typedef Vector<jssrcnote, 64> SrcNotesVector;

class CallOrNewEmitter;
class ClassEmitter;
class ElemOpEmitter;
class EmitterScope;
class NestableControl;
class PropertyEmitter;
class PropOpEmitter;
class TDZCheckCache;

struct MOZ_STACK_CLASS BytecodeEmitter
{
    SharedContext* const sc;      /* context shared between parsing and bytecode generation */

    ExclusiveContext* const cx;

    BytecodeEmitter* const parent;  /* enclosing function or global context */

    Rooted<JSScript*> script;       /* the JSScript we're ultimately producing */

    Rooted<LazyScript*> lazyScript; /* the lazy script if mode is LazyFunction,
                                        nullptr otherwise. */

    struct EmitSection {
        BytecodeVector code;        /* bytecode */
        SrcNotesVector notes;       /* source notes, see below */
        ptrdiff_t   lastNoteOffset; /* code offset for last source note */
        uint32_t    currentLine;    /* line number for tree-based srcnote gen */
        uint32_t    lastColumn;     /* zero-based column index on currentLine of
                                       last SRC_COLSPAN-annotated opcode */
        JumpTarget lastTarget;      // Last jump target emitted.

        EmitSection(ExclusiveContext* cx, uint32_t lineNum)
          : code(cx), notes(cx), lastNoteOffset(0), currentLine(lineNum), lastColumn(0),
            lastTarget{ -1 - ptrdiff_t(JSOP_JUMPTARGET_LENGTH) }
        {}
    };
    EmitSection prologue, main, *current;

    Parser<FullParseHandler>* const parser;

    PooledMapPtr<AtomIndexMap> atomIndices; /* literals indexed for mapping */
    unsigned        firstLine;      /* first line, for JSScript::initFromEmitter */

    uint32_t        maxFixedSlots;  /* maximum number of fixed frame slots so far */
    uint32_t        maxStackDepth;  /* maximum number of expression stack slots so far */

    int32_t         stackDepth;     /* current stack depth in script frame */

    uint32_t        arrayCompDepth; /* stack depth of array in comprehension */

    unsigned        emitLevel;      /* emitTree recursion level */

    uint32_t        bodyScopeIndex; /* index into scopeList of the body scope */

    EmitterScope*    varEmitterScope;
    NestableControl* innermostNestableControl;
    EmitterScope*    innermostEmitterScope_;
    TDZCheckCache*   innermostTDZCheckCache;

    /* field info for enclosing class */
    FieldInitializers fieldInitializers_;
    const FieldInitializers& getFieldInitializers() { return fieldInitializers_; }

#ifdef DEBUG
    bool unstableEmitterScope;

    friend class AutoCheckUnstableEmitterScope;
#endif

    EmitterScope* innermostEmitterScope() const {
        MOZ_ASSERT(!unstableEmitterScope);
        return innermostEmitterScopeNoCheck();
    }
    EmitterScope* innermostEmitterScopeNoCheck() const {
        return innermostEmitterScope_;
    }

    CGConstList      constList;      /* double and bigint values used by script */
    CGObjectList     objectList;     /* list of emitted objects */
    CGScopeList      scopeList;      /* list of emitted scopes */
    CGTryNoteList    tryNoteList;    /* list of emitted try notes */
    CGScopeNoteList  scopeNoteList;  /* list of emitted block scope notes */

    /*
     * For each yield or await op, map the yield and await index (stored as
     * bytecode operand) to the offset of the next op.
     */
    CGYieldAndAwaitOffsetList yieldAndAwaitOffsetList;

    uint16_t        typesetCount;   /* Number of JOF_TYPESET opcodes generated */

    bool            hasSingletons:1;    /* script contains singleton initializer JSOP_OBJECT */

    bool            hasTryFinally:1;    /* script contains finally block */

    bool            emittingRunOnceLambda:1; /* true while emitting a lambda which is only
                                                expected to run once. */

    bool isRunOnceLambda();

    enum EmitterMode {
        Normal,

        /*
         * Emit JSOP_GETINTRINSIC instead of JSOP_GETNAME and assert that
         * JSOP_GETNAME and JSOP_*GNAME don't ever get emitted. See the comment
         * for the field |selfHostingMode| in Parser.h for details.
         */
        SelfHosting,

        /*
         * Check the static scope chain of the root function for resolving free
         * variable accesses in the script.
         */
        LazyFunction
    };

    const EmitterMode emitterMode;

    mozilla::Maybe<uint32_t> scriptStartOffset;

    // The end location of a function body that is being emitted.
    mozilla::Maybe<uint32_t> functionBodyEndPos;

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destruction.
     */
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler>* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, uint32_t lineNum,
                    EmitterMode emitterMode = Normal,
                    FieldInitializers fieldInitializers = FieldInitializers::Invalid());

    // An alternate constructor that uses a TokenPos for the starting
    // line and that sets functionBodyEndPos as well.
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler>* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript,
                    TokenPos bodyPosition, EmitterMode emitterMode = Normal,
                    FieldInitializers fieldInitializers = FieldInitializers::Invalid());

    [[nodiscard]] bool init();

    template <typename T>
    T* findInnermostNestableControl() const;

    template <typename T, typename Predicate /* (T*) -> bool */>
    T* findInnermostNestableControl(Predicate predicate) const;

    NameLocation lookupName(JSAtom* name);

    // To implement Annex B and the formal parameter defaults scope semantics
    // requires accessing names that would otherwise be shadowed. This method
    // returns the access location of a name that is known to be bound in a
    // target scope.
    mozilla::Maybe<NameLocation> locationOfNameBoundInScope(JSAtom* name, EmitterScope* target);

    // Get the location of a name known to be bound in the function scope,
    // starting at the source scope.
    mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(JSAtom* name,
                                                                    EmitterScope* source);

    mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(JSAtom* name) {
        return locationOfNameBoundInFunctionScope(name, innermostEmitterScope());
    }

    void setVarEmitterScope(EmitterScope* emitterScope) {
        MOZ_ASSERT(emitterScope);
        MOZ_ASSERT(!varEmitterScope);
        varEmitterScope = emitterScope;
    }

    Scope* bodyScope() const { return scopeList.vector[bodyScopeIndex]; }
    Scope* outermostScope() const { return scopeList.vector[0]; }
    Scope* innermostScope() const;

    [[nodiscard]] MOZ_ALWAYS_INLINE
    bool makeAtomIndex(JSAtom* atom, uint32_t* indexp) {
        MOZ_ASSERT(atomIndices);
        AtomIndexMap::AddPtr p = atomIndices->lookupForAdd(atom);
        if (p) {
            *indexp = p->value();
            return true;
        }

        uint32_t index = atomIndices->count();
        if (!atomIndices->add(p, atom, index))
            return false;

        *indexp = index;
        return true;
    }

    bool isInLoop();
    [[nodiscard]] bool checkSingletonContext();

    // Check whether our function is in a run-once context (a toplevel
    // run-one script or a run-once lambda).
    [[nodiscard]] bool checkRunOnceContext();

    bool needsImplicitThis();

    [[nodiscard]] bool maybeSetDisplayURL();
    [[nodiscard]] bool maybeSetSourceMap();
    void tellDebuggerAboutCompiledScript(ExclusiveContext* cx);

    inline TokenStream& tokenStream();

    BytecodeVector& code() const { return current->code; }
    jsbytecode* code(ptrdiff_t offset) const { return current->code.begin() + offset; }
    ptrdiff_t offset() const { return current->code.end() - current->code.begin(); }
    ptrdiff_t prologueOffset() const { return prologue.code.end() - prologue.code.begin(); }
    void switchToMain() { current = &main; }
    void switchToPrologue() { current = &prologue; }
    bool inPrologue() const { return current == &prologue; }

    SrcNotesVector& notes() const { return current->notes; }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }
    unsigned lastColumn() const { return current->lastColumn; }

    // Check if the last emitted opcode is a jump target.
    bool lastOpcodeIsJumpTarget() const {
        return offset() - current->lastTarget.offset == ptrdiff_t(JSOP_JUMPTARGET_LENGTH);
    }

    // JumpTarget should not be part of the emitted statement, as they can be
    // aliased by multiple statements. If we included the jump target as part of
    // the statement we might have issues where the enclosing statement might
    // not contain all the opcodes of the enclosed statements.
    ptrdiff_t lastNonJumpTargetOffset() const {
        return lastOpcodeIsJumpTarget() ? current->lastTarget.offset : offset();
    }

    void setFunctionBodyEndPos(uint32_t pos) {
        functionBodyEndPos = mozilla::Some(pos);
    }

    void setScriptStartOffsetIfUnset(uint32_t pos) {
        if (scriptStartOffset.isNothing()) {
            scriptStartOffset = mozilla::Some(pos);
        }
    }

    bool reportError(ParseNode* pn, unsigned errorNumber, ...);
    bool reportError(const mozilla::Maybe<uint32_t>& maybeOffset, unsigned errorNumber, ...);
    bool reportExtraWarning(ParseNode* pn, unsigned errorNumber, ...);
    bool reportExtraWarning(const mozilla::Maybe<uint32_t>& maybeOffset, unsigned errorNumber, ...);
    bool reportStrictModeError(ParseNode* pn, unsigned errorNumber, ...);

    // If pn contains a useful expression, return true with *answer set to true.
    // If pn contains a useless expression, return true with *answer set to
    // false. Return false on error.
    //
    // The caller should initialize *answer to false and invoke this function on
    // an expression statement or similar subtree to decide whether the tree
    // could produce code that has any side effects.  For an expression
    // statement, we define useless code as code with no side effects, because
    // the main effect, the value left on the stack after the code executes,
    // will be discarded by a pop bytecode.
    [[nodiscard]] bool checkSideEffects(ParseNode* pn, bool* answer);

#ifdef DEBUG
    [[nodiscard]] bool checkStrictOrSloppy(JSOp op);
#endif

    // Append a new source note of the given type (and therefore size) to the
    // notes dynamic array, updating noteCount. Return the new note's index
    // within the array pointed at by current->notes as outparam.
    [[nodiscard]] bool newSrcNote(SrcNoteType type, unsigned* indexp = nullptr);
    [[nodiscard]] bool newSrcNote2(SrcNoteType type, ptrdiff_t offset, unsigned* indexp = nullptr);
    [[nodiscard]] bool newSrcNote3(SrcNoteType type, ptrdiff_t offset1, ptrdiff_t offset2,
                                  unsigned* indexp = nullptr);

    void copySrcNotes(jssrcnote* destination, uint32_t nsrcnotes);
    [[nodiscard]] bool setSrcNoteOffset(unsigned index, unsigned which, ptrdiff_t offset);

    // NB: this function can add at most one extra extended delta note.
    [[nodiscard]] bool addToSrcNoteDelta(jssrcnote* sn, ptrdiff_t delta);

    // Finish taking source notes in cx's notePool. If successful, the final
    // source note count is stored in the out outparam.
    [[nodiscard]] bool finishTakingSrcNotes(uint32_t* out);

    // Control whether emitTree emits a line number note.
    enum EmitLineNumberNote {
        EMIT_LINENOTE,
        SUPPRESS_LINENOTE
    };

    // Emit code for the tree rooted at pn.
    [[nodiscard]] bool emitTree(ParseNode* pn, ValueUsage valueUsage = ValueUsage::WantValue,
                               EmitLineNumberNote emitLineNote = EMIT_LINENOTE);

    // Emit code for the optional tree rooted at pn.
    [[nodiscard]] bool emitOptionalTree(ParseNode* pn,
                                       OptionalEmitter& oe,
                                       ValueUsage valueUsage = ValueUsage::WantValue);

    // Emit code for the tree rooted at pn with its own TDZ cache.
    [[nodiscard]] bool emitTreeInBranch(ParseNode* pn,
                                       ValueUsage valueUsage = ValueUsage::WantValue);

    // Emit global, eval, or module code for tree rooted at body. Always
    // encompasses the entire source.
    [[nodiscard]] bool emitScript(ParseNode* body);

    // Emit function code for the tree rooted at body.
    [[nodiscard]] bool emitFunctionScript(FunctionNode* funNode);

    void updateDepth(ptrdiff_t target);
    [[nodiscard]] bool updateLineNumberNotes(uint32_t offset);
    [[nodiscard]] bool updateSourceCoordNotes(uint32_t offset);

    JSOp strictifySetNameOp(JSOp op);

    [[nodiscard]] bool emitCheck(JSOp op, ptrdiff_t delta, ptrdiff_t* offset);

    // Emit one bytecode.
    [[nodiscard]] bool emit1(JSOp op);

    // Emit two bytecodes, an opcode (op) with a byte of immediate operand
    // (op1).
    [[nodiscard]] bool emit2(JSOp op, uint8_t op1);

    // Emit three bytecodes, an opcode with two bytes of immediate operands.
    [[nodiscard]] bool emit3(JSOp op, jsbytecode op1, jsbytecode op2);

    // Helper to emit JSOP_DUPAT. The argument is the value's depth on the
    // JS stack, as measured from the top.
    [[nodiscard]] bool emitDupAt(unsigned slotFromTop);

    // Helper to emit JSOP_POP or JSOP_POPN.
    [[nodiscard]] bool emitPopN(unsigned n);

    // Helper to emit JSOP_SWAP or JSOP_UNPICK.
    [[nodiscard]] bool emitUnpickN(unsigned n);

    // Helper to emit JSOP_CHECKISOBJ.
    [[nodiscard]] bool emitCheckIsObj(CheckIsObjectKind kind);

    // Helper to emit JSOP_CHECKISCALLABLE.
    [[nodiscard]] bool emitCheckIsCallable(CheckIsCallableKind kind);

    // Emit a bytecode followed by an uint16 immediate operand stored in
    // big-endian order.
    [[nodiscard]] bool emitUint16Operand(JSOp op, uint32_t operand);

    // Emit a bytecode followed by an uint32 immediate operand.
    [[nodiscard]] bool emitUint32Operand(JSOp op, uint32_t operand);

    // Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
    [[nodiscard]] bool emitN(JSOp op, size_t extra, ptrdiff_t* offset = nullptr);

    [[nodiscard]] bool emitNumberOp(double dval);

    [[nodiscard]] bool emitBigIntOp(BigInt* bigint);

    [[nodiscard]] bool emitThisLiteral(ThisLiteral* pn);
    [[nodiscard]] bool emitGetFunctionThis(ParseNode* pn);
    [[nodiscard]] bool emitGetFunctionThis(const mozilla::Maybe<uint32_t>& offset);
    [[nodiscard]] bool emitGetThisForSuperBase(UnaryNode* superBase);
    [[nodiscard]] bool emitSetThis(BinaryNode* setThisNode);
    [[nodiscard]] bool emitCheckDerivedClassConstructorReturn();

    // Handle jump opcodes and jump targets.
    [[nodiscard]] bool emitJumpTarget(JumpTarget* target);
    [[nodiscard]] bool emitJumpNoFallthrough(JSOp op, JumpList* jump);
    [[nodiscard]] bool emitJump(JSOp op, JumpList* jump);
    [[nodiscard]] bool emitBackwardJump(JSOp op, JumpTarget target, JumpList* jump,
                                       JumpTarget* fallthrough);
    void patchJumpsToTarget(JumpList jump, JumpTarget target);
    [[nodiscard]] bool emitJumpTargetAndPatch(JumpList jump);

    [[nodiscard]] bool emitCall(JSOp op, uint16_t argc,
                               const mozilla::Maybe<uint32_t>& sourceCoordOffset);
    [[nodiscard]] bool emitCall(JSOp op, uint16_t argc, ParseNode* pn = nullptr);
    [[nodiscard]] bool emitCallIncDec(UnaryNode* incDec);

    [[nodiscard]] bool emitLoopHead(ParseNode* nextpn, JumpTarget* top);
    [[nodiscard]] bool emitLoopEntry(ParseNode* nextpn, JumpList entryJump);

    [[nodiscard]] bool emitGoto(NestableControl* target, JumpList* jumplist,
                               SrcNoteType noteType = SRC_NULL);

    [[nodiscard]] bool emitIndex32(JSOp op, uint32_t index);
    [[nodiscard]] bool emitIndexOp(JSOp op, uint32_t index);

    [[nodiscard]] bool emitAtomOp(JSAtom* atom, JSOp op);
    [[nodiscard]] bool emitAtomOp(uint32_t atomIndex, JSOp op);

    [[nodiscard]] bool emitArrayLiteral(ListNode* array);
    [[nodiscard]] bool emitArray(ParseNode* arrayHead, uint32_t count, JSOp op);
    [[nodiscard]] bool emitArrayComp(ListNode* pn);

    [[nodiscard]] bool emitInternedScopeOp(uint32_t index, JSOp op);
    [[nodiscard]] bool emitInternedObjectOp(uint32_t index, JSOp op);
    [[nodiscard]] bool emitObjectOp(ObjectBox* objbox, JSOp op);
    [[nodiscard]] bool emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op);
    [[nodiscard]] bool emitRegExp(uint32_t index);

    [[nodiscard]] MOZ_NEVER_INLINE bool emitFunction(FunctionNode* funNode,
                                                    bool needsProto = false,
                                                    ListNode* classContentsIfConstructor = nullptr);
    [[nodiscard]] MOZ_NEVER_INLINE bool emitObject(ListNode* objNode);

    [[nodiscard]] bool replaceNewInitWithNewObject(JSObject* obj, ptrdiff_t offset);

    [[nodiscard]] bool emitHoistedFunctionsInList(ListNode* stmtList);

    [[nodiscard]] bool emitPropertyList(ListNode* obj, PropertyEmitter& pe,
                                       PropListType type);

    enum class FieldPlacement { Instance, Static };
    FieldInitializers setupFieldInitializers(ListNode* classMembers, FieldPlacement placement);
    [[nodiscard]] bool emitCreateFieldKeys(ListNode* obj, FieldPlacement placement);
    [[nodiscard]] bool emitCreateFieldInitializers(ClassEmitter& ce, ListNode* obj, FieldPlacement placement);
    const FieldInitializers& findFieldInitializersForCall();
    [[nodiscard]] bool emitInitializeInstanceFields();
    [[nodiscard]] bool emitInitializeStaticFields(ListNode* classMembers);

    // To catch accidental misuse, emitUint16Operand/emit3 assert that they are
    // not used to unconditionally emit JSOP_GETLOCAL. Variable access should
    // instead be emitted using EmitVarOp. In special cases, when the caller
    // definitely knows that a given local slot is unaliased, this function may be
    // used as a non-asserting version of emitUint16Operand.
    [[nodiscard]] bool emitLocalOp(JSOp op, uint32_t slot);

    [[nodiscard]] bool emitArgOp(JSOp op, uint16_t slot);
    [[nodiscard]] bool emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec);

    [[nodiscard]] bool emitGetNameAtLocation(JSAtom* name, const NameLocation& loc);
    [[nodiscard]] bool emitGetName(JSAtom* name) {
        return emitGetNameAtLocation(name, lookupName(name));
    }
    [[nodiscard]] bool emitGetName(ParseNode* pn);

    [[nodiscard]] bool emitTDZCheckIfNeeded(JSAtom* name, const NameLocation& loc);

    [[nodiscard]] bool emitNameIncDec(UnaryNode* incDec);

    [[nodiscard]] bool emitDeclarationList(ListNode* declList);
    [[nodiscard]] bool emitSingleDeclaration(ParseNode* declList, ParseNode* decl,
                                            ParseNode* initializer);

    [[nodiscard]] bool emitNewInit(JSProtoKey key);
    [[nodiscard]] bool emitSingletonInitialiser(ParseNode* pn);

    [[nodiscard]] bool emitPrepareIteratorResult();
    [[nodiscard]] bool emitFinishIteratorResult(bool done);
    [[nodiscard]] bool iteratorResultShape(unsigned* shape);
    [[nodiscard]] bool emitToIteratorResult(bool done);

    [[nodiscard]] bool emitGetDotGeneratorInInnermostScope() {
        return emitGetDotGeneratorInScope(*innermostEmitterScope());
    }
    [[nodiscard]] bool emitGetDotGeneratorInScope(EmitterScope& currentScope);

    [[nodiscard]] bool emitInitialYield(UnaryNode* yieldNode);
    [[nodiscard]] bool emitYield(UnaryNode* yieldNode);
    [[nodiscard]] bool emitYieldOp(JSOp op);
    [[nodiscard]] bool emitYieldStar(ParseNode* iter);
    [[nodiscard]] bool emitAwaitInInnermostScope() {
        return emitAwaitInScope(*innermostEmitterScope());
    }
    [[nodiscard]] bool emitAwaitInInnermostScope(UnaryNode* awaitNode);
    [[nodiscard]] bool emitAwaitInScope(EmitterScope& currentScope);
    [[nodiscard]] bool emitPropLHS(PropertyAccess* prop);
    [[nodiscard]] bool emitPropIncDec(UnaryNode* incDec);

    [[nodiscard]] bool emitAsyncWrapperLambda(unsigned index, bool isArrow);
    [[nodiscard]] bool emitAsyncWrapper(unsigned index, bool needsHomeObject, bool isArrow,
                                       bool isStarGenerator);

    [[nodiscard]] bool emitComputedPropertyName(UnaryNode* computedPropName);

    // Emit bytecode to put operands for a JSOP_GETELEM/CALLELEM/SETELEM/DELELEM
    // opcode onto the stack in the right order. In the case of SETELEM, the
    // value to be assigned must already be pushed.
    enum class EmitElemOption { Get, Set, Call, IncDec, CompoundAssign, Ref };
    [[nodiscard]] bool emitElemOperands(PropertyByValue* elem, EmitElemOption opts);

    [[nodiscard]] bool emitElemObjAndKey(PropertyByValue* elem, bool isSuper, ElemOpEmitter& eoe);
    [[nodiscard]] bool emitElemOpBase(JSOp op);
    [[nodiscard]] bool emitElemIncDec(UnaryNode* incDec);

    [[nodiscard]] bool emitCatch(TernaryNode* catchNode);
    [[nodiscard]] bool emitIf(TernaryNode* ifNode);
    [[nodiscard]] bool emitWith(BinaryNode* withNode);

    [[nodiscard]] MOZ_NEVER_INLINE bool emitLabeledStatement(const LabeledStatement* pn);
    [[nodiscard]] MOZ_NEVER_INLINE bool emitLexicalScope(LexicalScopeNode* lexicalScope);
    [[nodiscard]] bool emitLexicalScopeBody(ParseNode* body,
                                           EmitLineNumberNote emitLineNote = EMIT_LINENOTE);
    [[nodiscard]] MOZ_NEVER_INLINE bool emitSwitch(SwitchStatement* switchStmt);
    [[nodiscard]] MOZ_NEVER_INLINE bool emitTry(TryNode* tryNode);

    // emitDestructuringLHSRef emits the lhs expression's reference.
    // If the lhs expression is object property |OBJ.prop|, it emits |OBJ|.
    // If it's object element |OBJ[ELEM]|, it emits |OBJ| and |ELEM|.
    // If there's nothing to evaluate for the reference, it emits nothing.
    // |emitted| parameter receives the number of values pushed onto the stack.
    [[nodiscard]] bool emitDestructuringLHSRef(ParseNode* target, size_t* emitted);

    // emitSetOrInitializeDestructuring assumes the lhs expression's reference
    // and the to-be-destructured value has been pushed on the stack.  It emits
    // code to destructure a single lhs expression (either a name or a compound
    // []/{} expression).
    [[nodiscard]] bool emitSetOrInitializeDestructuring(ParseNode* target, DestructuringFlavor flav);

    // emitDestructuringObjRestExclusionSet emits the property exclusion set
    // for the rest-property in an object pattern.
    [[nodiscard]] bool emitDestructuringObjRestExclusionSet(ListNode* pattern);

    // emitDestructuringOps assumes the to-be-destructured value has been
    // pushed on the stack and emits code to destructure each part of a [] or
    // {} lhs expression.
    [[nodiscard]] bool emitDestructuringOps(ListNode* pattern, DestructuringFlavor flav);
    [[nodiscard]] bool emitDestructuringOpsArray(ListNode* pattern, DestructuringFlavor flav);
    [[nodiscard]] bool emitDestructuringOpsObject(ListNode* pattern, DestructuringFlavor flav);

    typedef bool
    (*DestructuringDeclEmitter)(BytecodeEmitter* bce, ParseNode* pn);

    // Throw a TypeError if the value atop the stack isn't convertible to an
    // object, with no overall effect on the stack.
    [[nodiscard]] bool emitRequireObjectCoercible();

    enum class CopyOption {
        Filtered, Unfiltered
    };

    // Calls either the |CopyDataProperties| or the
    // |CopyDataPropertiesUnfiltered| intrinsic function, consumes three (or
    // two in the latter case) elements from the stack.
    [[nodiscard]] bool emitCopyDataProperties(CopyOption option);

    // emitIterator expects the iterable to already be on the stack.
    // It will replace that stack value with the corresponding iterator
    [[nodiscard]] bool emitIterator();

    [[nodiscard]] bool emitAsyncIterator();

    // XXX currently used only by OptionalEmitter, research still required
    // to identify when this was introduced in m-c.
    [[nodiscard]] bool emitPushNotUndefinedOrNull();

    // Pops iterator from the top of the stack. Pushes the result of |.next()|
    // onto the stack.
    [[nodiscard]] bool emitIteratorNext(ParseNode* pn, IteratorKind kind = IteratorKind::Sync,
                                       bool allowSelfHosted = false);
    [[nodiscard]] bool emitIteratorCloseInScope(EmitterScope& currentScope,
                                               IteratorKind iterKind = IteratorKind::Sync,
                                               CompletionKind completionKind = CompletionKind::Normal,
                                               bool allowSelfHosted = false);
    [[nodiscard]] bool emitIteratorCloseInInnermostScope(IteratorKind iterKind = IteratorKind::Sync,
                                                        CompletionKind completionKind = CompletionKind::Normal,
                                                        bool allowSelfHosted = false) {
        return emitIteratorCloseInScope(*innermostEmitterScope(), iterKind, completionKind,
                                        allowSelfHosted);
    }

    template <typename InnerEmitter>
    [[nodiscard]] bool wrapWithDestructuringIteratorCloseTryNote(int32_t iterDepth,
                                                                InnerEmitter emitter);

    // Check if the value on top of the stack is "undefined". If so, replace
    // that value on the stack with the value defined by |defaultExpr|.
    // |pattern| is a lhs node of the default expression.  If it's an
    // identifier and |defaultExpr| is an anonymous function, |SetFunctionName|
    // is called at compile time.
    [[nodiscard]] bool emitDefault(ParseNode* defaultExpr, ParseNode* pattern);

    [[nodiscard]] bool setOrEmitSetFunName(ParseNode* maybeFun, HandleAtom name);
    [[nodiscard]] bool setFunName(JSFunction* fun, JSAtom* name);
    [[nodiscard]] bool emitSetClassConstructorName(JSAtom* name);
    [[nodiscard]] bool emitSetFunctionNameFromStack(uint8_t offset);

    [[nodiscard]] bool emitInitializer(ParseNode* initializer, ParseNode* pattern);

    [[nodiscard]] bool emitCallSiteObject(CallSiteNode* callSiteObj);
    [[nodiscard]] bool emitTemplateString(ListNode* templateString);
    [[nodiscard]] bool emitAssignmentOrInit(ParseNodeKind kind, JSOp compoundOp,
                                           ParseNode* lhs, ParseNode* rhs);
    [[nodiscard]] bool emitShortCircuitAssignment(ParseNodeKind kind, JSOp op,
                                                 ParseNode* lhs, ParseNode* rhs);

    [[nodiscard]] bool emitReturn(UnaryNode* returnNode);
    [[nodiscard]] bool emitStatement(UnaryNode* exprStmt);
    [[nodiscard]] bool emitStatementList(ListNode* stmtList);

    [[nodiscard]] bool emitDeleteName(UnaryNode* deleteNode);
    [[nodiscard]] bool emitDeleteProperty(UnaryNode* deleteNode);
    [[nodiscard]] bool emitDeleteElement(UnaryNode* deleteNode);
    [[nodiscard]] bool emitDeleteExpression(UnaryNode* deleteNode);

    // Optional methods which emit Optional Jump Target
    [[nodiscard]] bool emitOptionalChain(UnaryNode* optionalChain,
                                        ValueUsage valueUsage);
    [[nodiscard]] bool emitCalleeAndThisForOptionalChain(UnaryNode* optionalChain,
                                                        ParseNode* callNode,
                                                        CallOrNewEmitter& cone);
    [[nodiscard]] bool emitDeleteOptionalChain(UnaryNode* deleteNode);

    // Optional methods which emit a shortCircuit jump. They need to be called by
    // a method which emits an Optional Jump Target, see below.
    [[nodiscard]] bool emitOptionalDotExpression(PropertyAccessBase* prop,
                                                PropOpEmitter& poe, bool isSuper,
                                                OptionalEmitter& oe);
    [[nodiscard]] bool emitOptionalElemExpression(PropertyByValueBase* elem,
                                                 ElemOpEmitter& eoe, bool isSuper,
                                                 OptionalEmitter& oe);
    [[nodiscard]] bool emitOptionalCall(BinaryNode* callNode,
                                       OptionalEmitter& oe,
                                       ValueUsage valueUsage);
    [[nodiscard]] bool emitDeletePropertyInOptChain(PropertyAccessBase* propExpr,
                                                   OptionalEmitter& oe);
    [[nodiscard]] bool emitDeleteElementInOptChain(PropertyByValueBase* elemExpr,
                                                  OptionalEmitter& oe);

    // |op| must be JSOP_TYPEOF or JSOP_TYPEOFEXPR.
    [[nodiscard]] bool emitTypeof(UnaryNode* typeofNode, JSOp op);

    [[nodiscard]] bool emitUnary(UnaryNode* unaryNode);
    [[nodiscard]] bool emitRightAssociative(ListNode* node);
    [[nodiscard]] bool emitLeftAssociative(ListNode* node);
    [[nodiscard]] bool emitLogical(ListNode* node);
    [[nodiscard]] bool emitSequenceExpr(ListNode* node,
                                       ValueUsage valueUsage = ValueUsage::WantValue);

    [[nodiscard]] MOZ_NEVER_INLINE bool emitIncOrDec(UnaryNode* incDec);

    [[nodiscard]] bool emitConditionalExpression(ConditionalExpression& conditional,
                                                ValueUsage valueUsage = ValueUsage::WantValue);

    [[nodiscard]] bool isRestParameter(ParseNode* pn);
    [[nodiscard]] bool emitOptimizeSpread(ParseNode* arg0, JumpList* jmp, bool* emitted);

    [[nodiscard]] ParseNode* getCoordNode(ParseNode* callNode, ParseNode* calleeNode,
                                         ListNode* argsList);
    [[nodiscard]] bool emitArguments(ListNode* argsList, bool isCall, bool isSpread,
                                    CallOrNewEmitter& cone);
    [[nodiscard]] bool emitCallOrNew(BinaryNode* pn,
                                    ValueUsage valueUsage = ValueUsage::WantValue);
    [[nodiscard]] bool emitCalleeAndThis(ParseNode* callNode,
                                        ParseNode* calleeNode,
                                        CallOrNewEmitter& cone);
    [[nodiscard]] bool emitOptionalCalleeAndThis(ParseNode* callNode,
                                                ParseNode* calleeNode,
                                                CallOrNewEmitter& cone,
                                                OptionalEmitter& oe);

    [[nodiscard]] bool emitSelfHostedCallFunction(BinaryNode* callNode);
    [[nodiscard]] bool emitSelfHostedResumeGenerator(BinaryNode* callNode);
    [[nodiscard]] bool emitSelfHostedForceInterpreter(ParseNode* pn);
    [[nodiscard]] bool emitSelfHostedAllowContentIter(BinaryNode* callNode);

    [[nodiscard]] bool emitComprehensionFor(ForNode* forNode);
    [[nodiscard]] bool emitComprehensionForIn(ForNode* forNode);
    [[nodiscard]] bool emitComprehensionForInOrOfVariables(ParseNode* pn, bool* lexicalScope);
    [[nodiscard]] bool emitComprehensionForOf(ForNode* forNode);

    [[nodiscard]] bool emitDo(BinaryNode* doNode);
    [[nodiscard]] bool emitWhile(BinaryNode* whileNode);

    [[nodiscard]] bool emitFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope = nullptr);
    [[nodiscard]] bool emitCStyleFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);
    [[nodiscard]] bool emitForIn(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);
    [[nodiscard]] bool emitForOf(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);

    [[nodiscard]] bool emitInitializeForInOrOfTarget(TernaryNode* forHead);

    [[nodiscard]] bool emitBreak(PropertyName* label);
    [[nodiscard]] bool emitContinue(PropertyName* label);

    [[nodiscard]] bool emitFunctionFormalParameters(ListNode* paramsBody);
    [[nodiscard]] bool emitInitializeFunctionSpecialNames();
    [[nodiscard]] bool emitLexicalInitialization(NameNode* pn);
    [[nodiscard]] bool emitLexicalInitialization(JSAtom* name);

    // Emit bytecode for the spread operator.
    //
    // emitSpread expects the current index (I) of the array, the array itself
    // and the iterator to be on the stack in that order (iterator on the bottom).
    // It will pop the iterator and I, then iterate over the iterator by calling
    // |.next()| and put the results into the I-th element of array with
    // incrementing I, then push the result I (it will be original I +
    // iteration count). The stack after iteration will look like |ARRAY INDEX|.
    [[nodiscard]] bool emitSpread(bool allowSelfHosted = false);

    [[nodiscard]] bool emitClass(ClassNode* classNode);
};

class MOZ_RAII AutoCheckUnstableEmitterScope {
#ifdef DEBUG
    bool prev_;
    BytecodeEmitter* bce_;
#endif

  public:
    AutoCheckUnstableEmitterScope() = delete;
    explicit AutoCheckUnstableEmitterScope(BytecodeEmitter* bce)
#ifdef DEBUG
      : bce_(bce)
#endif
    {
#ifdef DEBUG
        prev_ = bce_->unstableEmitterScope;
        bce_->unstableEmitterScope = true;
#endif
    }
    ~AutoCheckUnstableEmitterScope() {
#ifdef DEBUG
        bce_->unstableEmitterScope = prev_;
#endif
    }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeEmitter_h */
