/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_RegExp_h
#define builtin_RegExp_h

#include "vm/RegExpObject.h"

/*
 * The following builtin natives are extern'd for pointer comparison in
 * other parts of the engine.
 */

namespace js {

JSObject*
InitRegExpClass(JSContext* cx, HandleObject obj);

/*
 * Legacy behavior of ExecuteRegExp(), which is baked into the JSAPI.
 *
 * |res| may be nullptr if the RegExpStatics are not to be updated.
 * |input| may be nullptr if there is no JSString corresponding to
 * |chars| and |length|.
 */
[[nodiscard]] bool
ExecuteRegExpLegacy(JSContext* cx, RegExpStatics* res, Handle<RegExpObject*> reobj,
                    HandleLinearString input, size_t* lastIndex, bool test,
                    MutableHandleValue rval);

/* Translation from MatchPairs to a JS array in regexp_exec()'s output format. */
[[nodiscard]] bool
CreateRegExpMatchResult(JSContext* cx, RegExpShared& re,
                        HandleString input, const MatchPairs& matches,
                        MutableHandleValue rval);

[[nodiscard]] extern bool
RegExpMatcher(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
RegExpMatcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                 int32_t lastIndex, MatchPairs* maybeMatches, MutableHandleValue output);

[[nodiscard]] extern bool
RegExpSearcher(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
RegExpSearcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                  int32_t lastIndex, MatchPairs* maybeMatches, int32_t* result);

[[nodiscard]] extern bool
RegExpTester(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
RegExpTesterRaw(JSContext* cx, HandleObject regexp, HandleString input,
                int32_t lastIndex, int32_t* endIndex);

[[nodiscard]] extern bool
intrinsic_GetElemBaseForLambda(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
intrinsic_GetStringDataProperty(JSContext* cx, unsigned argc, Value* vp);

/*
 * The following functions are for use by self-hosted code.
 */

/*
 * Behaves like RegExp(pattern, flags).
 * |pattern| should be a RegExp object, |flags| should be a raw integer value.
 * Must be called without |new|.
 * Dedicated function for RegExp.prototype[@@split] optimized path.
 */
[[nodiscard]] extern bool
regexp_construct_raw_flags(JSContext* cx, unsigned argc, Value* vp);

/*
 * Clone given RegExp object, inheriting pattern and flags, ignoring other
 * properties.
 */
[[nodiscard]] extern bool
regexp_clone(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
IsRegExp(JSContext* cx, HandleValue value, bool* result);

[[nodiscard]] extern bool
RegExpCreate(JSContext* cx, HandleValue pattern, HandleValue flags, MutableHandleValue rval);

[[nodiscard]] extern bool
RegExpPrototypeOptimizable(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
RegExpPrototypeOptimizableRaw(JSContext* cx, JSObject* proto);

[[nodiscard]] extern bool
RegExpInstanceOptimizable(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
RegExpInstanceOptimizableRaw(JSContext* cx, JSObject* obj, JSObject* proto);

[[nodiscard]] extern bool
RegExpGetSubstitution(JSContext* cx, HandleArrayObject matchResult, HandleLinearString string,
                      size_t position, HandleLinearString replacement, size_t firstDollarIndex,
                      HandleValue namedCaptures, MutableHandleValue rval);

[[nodiscard]] extern bool
GetFirstDollarIndex(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] extern bool
GetFirstDollarIndexRaw(JSContext* cx, HandleString str, int32_t* index);

extern int32_t
GetFirstDollarIndexRawFlat(JSLinearString* text);

// RegExp ClassSpec members used in RegExpObject.cpp.
[[nodiscard]] extern bool
regexp_construct(JSContext* cx, unsigned argc, Value* vp);
extern const JSPropertySpec regexp_static_props[];
extern const JSPropertySpec regexp_properties[];
extern const JSFunctionSpec regexp_methods[];

// Used in RegExpObject::isOriginalFlagGetter.
[[nodiscard]] extern bool
regexp_hasIndices(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_global(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_ignoreCase(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_multiline(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_dotAll(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_sticky(JSContext* cx, unsigned argc, JS::Value* vp);
[[nodiscard]] extern bool
regexp_unicode(JSContext* cx, unsigned argc, JS::Value* vp);

} /* namespace js */

#endif /* builtin_RegExp_h */
