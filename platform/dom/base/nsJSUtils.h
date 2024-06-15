/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSUtils_h__
#define nsJSUtils_h__

/**
 * This is not a generated file. It contains common utility functions 
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "nsString.h"

class nsIScriptContext;
class nsIScriptGlobalObject;

namespace mozilla {
namespace dom {
class AutoJSAPI;
class Element;
} // namespace dom
} // namespace mozilla

class nsJSUtils
{
public:
  static bool GetCallingLocation(JSContext* aContext, nsACString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);
  static bool GetCallingLocation(JSContext* aContext, nsAString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);

  static nsIScriptGlobalObject *GetStaticScriptGlobal(JSObject* aObj);

  static nsIScriptContext *GetStaticScriptContext(JSObject* aObj);

  /**
   * Retrieve the inner window ID based on the given JSContext.
   *
   * @param JSContext aContext
   *        The JSContext from which you want to find the inner window ID.
   *
   * @returns uint64_t the inner window ID.
   */
  static uint64_t GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext);

  static nsresult CompileFunction(mozilla::dom::AutoJSAPI& jsapi,
                                  JS::AutoObjectVector& aScopeChain,
                                  JS::CompileOptions& aOptions,
                                  const nsACString& aName,
                                  uint32_t aArgCount,
                                  const char** aArgArray,
                                  const nsAString& aBody,
                                  JSObject** aFunctionObject);

  // ExecutionContext is used to switch compartment.
  class MOZ_STACK_CLASS ExecutionContext {
    JSContext* mCx;

    // Handles switching to our global's compartment.
    JSAutoCompartment mCompartment;

    // Set to a valid handle if a return value is expected.
    JS::Rooted<JS::Value> mRetValue;

    // Scope chain in which the execution takes place.
    JS::AutoObjectVector mScopeChain;

    // The compiled script.
    JS::Rooted<JSScript*> mScript;

    // returned value forwarded when we have to interupt the execution eagerly
    // with mSkip.
    nsresult mRv;

    // Used to skip upcoming phases in case of a failure.  In such case the
    // result is carried by mRv.
    bool mSkip;

    // Should the result be serialized before being returned.
    bool mCoerceToString;

    // Encode the bytecode before it is being executed.
    bool mEncodeBytecode;

#ifdef DEBUG
    // Should we set the return value.
    bool mWantsReturnValue;

    bool mExpectScopeChain;

    bool mScriptUsed;
#endif

   public:

    // Enter compartment in which the code would be executed.  The JSContext
    // must come from an AutoEntryScript that has had
    // TakeOwnershipOfErrorReporting() called on it.
    ExecutionContext(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

    ExecutionContext(const ExecutionContext&) = delete;
    ExecutionContext(ExecutionContext&&) = delete;

    ~ExecutionContext() {
      // This flag is reset when the returned value is extracted.
      MOZ_ASSERT_IF(!mSkip, !mWantsReturnValue);

      // If encoding was started we expect the script to have been
      // used when ending the encoding.
      MOZ_ASSERT_IF(mEncodeBytecode && mScript && mRv == NS_OK, mScriptUsed);
    }

    // The returned value would be converted to a string if the
    // |aCoerceToString| is flag set.
    ExecutionContext& SetCoerceToString(bool aCoerceToString) {
      mCoerceToString = aCoerceToString;
      return *this;
    }

    // When set, this flag records and encodes the bytecode as soon as it is
    // being compiled, and before it is being executed. The bytecode can then be
    // requested by using |JS::FinishIncrementalEncoding| with the mutable
    // handle |aScript| argument of |CompileAndExec| or |JoinAndExec|.
    ExecutionContext& SetEncodeBytecode(bool aEncodeBytecode) {
      mEncodeBytecode = aEncodeBytecode;
      return *this;
    }

    // Set the scope chain in which the code should be executed.
    void SetScopeChain(const JS::AutoObjectVector& aScopeChain);

    // After getting a notification that an off-thread compilation terminated,
    // this function will take the result of the parser and move it to the main
    // thread.
    [[nodiscard]] nsresult JoinCompile(void** aOffThreadToken);

    // Compile a script contained in a SourceText.
    nsresult Compile(JS::CompileOptions& aCompileOptions,
                     JS::SourceBufferHolder& aSrcBuf);

    // Compile a script contained in a string.
    nsresult Compile(JS::CompileOptions& aCompileOptions,
                     const nsAString& aScript);

    // Decode a script contained in a buffer.
    nsresult Decode(JS::CompileOptions& aCompileOptions,
                    mozilla::Vector<uint8_t>& aBytecodeBuf,
                    size_t aBytecodeIndex);

    // After getting a notification that an off-thread decoding terminated, this
    // function will get the result of the decoder and move it to the main
    // thread.
    nsresult JoinDecode(void** aOffThreadToken);

    // Get a successfully compiled script.
    JSScript* GetScript();

    // Execute the compiled script and ignore the return value.
    [[nodiscard]] nsresult ExecScript();

    // Execute the compiled script a get the return value.
    //
    // Copy the returned value into the mutable handle argument. In case of a
    // evaluation failure either during the execution or the conversion of the
    // result to a string, the nsresult is be set to the corresponding result
    // code and the mutable handle argument remains unchanged.
    //
    // The value returned in the mutable handle argument is part of the
    // compartment given as argument to the ExecutionContext constructor. If the
    // caller is in a different compartment, then the out-param value should be
    // wrapped by calling |JS_WrapValue|.
    [[nodiscard]] nsresult
    ExtractReturnValue(JS::MutableHandle<JS::Value> aRetValue);

    [[nodiscard]] nsresult ExecScript(JS::MutableHandle<JS::Value> aRetValue);
  };

  static nsresult CompileModule(JSContext* aCx,
                                JS::SourceBufferHolder& aSrcBuf,
                                JS::Handle<JSObject*> aEvaluationGlobal,
                                JS::CompileOptions &aCompileOptions,
                                JS::MutableHandle<JSObject*> aModule);

  static nsresult ModuleInstantiate(JSContext* aCx,
                                    JS::Handle<JSObject*> aModule);

  static nsresult ModuleEvaluate(JSContext* aCx,
                                 JS::Handle<JSObject*> aModule);

  // Returns false if an exception got thrown on aCx.  Passing a null
  // aElement is allowed; that wil produce an empty aScopeChain.
  static bool GetScopeChainForElement(JSContext* aCx,
                                      mozilla::dom::Element* aElement,
                                      JS::AutoObjectVector& aScopeChain);

  static void ResetTimeZone();
};

template<typename T>
inline bool
AssignJSString(JSContext *cx, T &dest, JSString *s)
{
  size_t len = js::GetStringLength(s);
  static_assert(js::MaxStringLength < (1 << 28),
                "Shouldn't overflow here or in SetCapacity");
  if (MOZ_UNLIKELY(!dest.SetLength(len, mozilla::fallible))) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  return js::CopyStringChars(cx, dest.BeginWriting(), s, len);
}

inline void
AssignJSFlatString(nsAString &dest, JSFlatString *s)
{
  size_t len = js::GetFlatStringLength(s);
  static_assert(js::MaxStringLength < (1 << 28),
                "Shouldn't overflow here or in SetCapacity");
  dest.SetLength(len);
  js::CopyFlatStringChars(dest.BeginWriting(), s, len);
}

class nsAutoJSString : public nsAutoString
{
public:

  /**
   * nsAutoJSString should be default constructed, which leaves it empty
   * (this->IsEmpty()), and initialized with one of the init() methods below.
   */
  nsAutoJSString() {}

  bool init(JSContext* aContext, JSString* str)
  {
    return AssignJSString(aContext, *this, str);
  }

  bool init(JSContext* aContext, const JS::Value &v)
  {
    if (v.isString()) {
      return init(aContext, v.toString());
    }

    // Stringify, making sure not to run script.
    JS::Rooted<JSString*> str(aContext);
    if (v.isObject()) {
      str = JS_NewStringCopyZ(aContext, "[Object]");
    } else {
      JS::Rooted<JS::Value> rootedVal(aContext, v);
      str = JS::ToString(aContext, rootedVal);
    }

    return str && init(aContext, str);
  }

  bool init(JSContext* aContext, jsid id)
  {
    JS::Rooted<JS::Value> v(aContext);
    return JS_IdToValue(aContext, id, &v) && init(aContext, v);
  }

  bool init(const JS::Value &v);

  ~nsAutoJSString() {}
};

#endif /* nsJSUtils_h__ */
