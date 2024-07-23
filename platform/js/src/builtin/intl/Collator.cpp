/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Collator implementation. */

#include "builtin/intl/Collator.h"

#include "mozilla/Assertions.h"
#include "mozilla/Span.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/LanguageTag.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/SharedIntlData.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using namespace js;
using js::intl::IcuLocale;
using js::intl::ReportInternalError;
using js::intl::SharedIntlData;
using js::intl::StringsAreEqual;

/******************** Collator ********************/

const ClassOps CollatorObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    CollatorObject::finalize
};

const Class CollatorObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(CollatorObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &CollatorObject::classOps_
};

#if JS_HAS_TOSOURCE
static bool
collator_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Collator);
    return true;
}
#endif

static const JSFunctionSpec collator_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_Collator_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec collator_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_Collator_resolvedOptions", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, collator_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 10.1.2 Intl.Collator([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
Collator(JSContext* cx, const CallArgs& args)
{
    // Step 1 (Handled by OrdinaryCreateFromConstructor fallback code).

    // Steps 2-5 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
    RootedObject proto(cx);
    if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    if (!proto) {
        proto = GlobalObject::getOrCreateCollatorPrototype(cx, cx->global());
        if (!proto)
            return false;
    }

    Rooted<CollatorObject*> collator(cx, NewObjectWithGivenProto<CollatorObject>(cx, proto));
    if (!collator)
        return false;

    collator->setReservedSlot(CollatorObject::INTERNALS_SLOT, NullValue());
    collator->setReservedSlot(CollatorObject::UCOLLATOR_SLOT, PrivateValue(nullptr));

    RootedValue locales(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    // Step 6.
    if (!intl::InitializeObject(cx, collator, cx->names().InitializeCollator, locales, options))
        return false;

    args.rval().setObject(*collator);
    return true;
}

static bool
Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return Collator(cx, args);
}

bool
js::intl_Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());

    return Collator(cx, args);
}

void
js::CollatorObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    const Value& slot = obj->as<CollatorObject>().getReservedSlot(CollatorObject::UCOLLATOR_SLOT);
    if (UCollator* coll = static_cast<UCollator*>(slot.toPrivate()))
        ucol_close(coll);
}

JSObject*
js::CreateCollatorPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, GlobalObject::createConstructor(cx, &Collator, cx->names().Collator,
                                                            0));
    if (!ctor)
        return nullptr;

    RootedObject proto(cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
    if (!proto)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 10.2.2
    if (!JS_DefineFunctions(cx, ctor, collator_static_methods))
        return nullptr;

    // 10.3.2 and 10.3.3
    if (!JS_DefineFunctions(cx, proto, collator_methods))
        return nullptr;

    /*
     * Install the getter for Collator.prototype.compare, which returns a bound
     * comparison function for the specified Collator object (suitable for
     * passing to methods like Array.prototype.sort).
     */
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().CollatorCompareGet, &getter))
        return nullptr;
    if (!DefineProperty(cx, proto, cx->names().compare, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().Collator, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_availableCollations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucol_getKeywordValuesForLocale("co", locale.ptr(), false, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    uint32_t count = uenum_count(values, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    RootedObject collations(cx, NewDenseEmptyArray(cx));
    if (!collations)
        return false;

    uint32_t index = 0;

    // The first element of the collations array must be |null| per
    // ES2017 Intl, 10.2.3 Internal Slots.
    if (!DefineElement(cx, collations, index++, NullHandleValue))
        return false;

    RootedValue element(cx);

    for (uint32_t i = 0; i < count; i++) {
        const char* collation = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        // Per ECMA-402, 10.2.3, we don't include standard and search:
        // "The values 'standard' and 'search' must not be used as elements in
        // any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co
        // array."
        if (StringsAreEqual(collation, "standard") || StringsAreEqual(collation, "search"))
            continue;

        // ICU returns old-style keyword values; map them to BCP 47 equivalents.
        JSString* jscollation = JS_NewStringCopyZ(cx, uloc_toUnicodeLocaleType("co", collation));
        if (!jscollation)
            return false;
        element = StringValue(jscollation);
        if (!DefineElement(cx, collations, index++, element))
            return false;
    }

    args.rval().setObject(*collations);
    return true;
}

/**
 * Returns a new UCollator with the locale and collation options
 * of the given Collator.
 */
static UCollator*
NewUCollator(JSContext* cx, Handle<CollatorObject*> collator)
{
    RootedValue value(cx);

    RootedObject internals(cx, intl::GetInternalsObject(cx, collator));
    if (!internals)
        return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // UCollator options with default values.
    UColAttributeValue uStrength = UCOL_DEFAULT;
    UColAttributeValue uCaseLevel = UCOL_OFF;
    UColAttributeValue uAlternate = UCOL_DEFAULT;
    UColAttributeValue uNumeric = UCOL_OFF;
    // Normalization is always on to meet the canonical equivalence requirement.
    UColAttributeValue uNormalization = UCOL_ON;
    UColAttributeValue uCaseFirst = UCOL_DEFAULT;

    if (!GetProperty(cx, internals, internals, cx->names().usage, &value))
        return nullptr;
    JSAutoByteString usage(cx, value.toString());
    if (!usage)
        return nullptr;
    if (StringsAreEqual(usage, "search")) {
        // ICU expects search as a Unicode locale extension on locale.
        intl::LanguageTag tag(cx);
        if (!intl::LanguageTagParser::parse(
                cx, mozilla::MakeCStringSpan(locale.ptr()), tag)) {
            return nullptr;
        }

        JS::RootedVector<intl::UnicodeExtensionKeyword> keywords(cx);

        if (!keywords.emplaceBack("co", cx->names().search)) {
            return nullptr;
        }

        // |ApplyUnicodeExtensionToTag| applies the new keywords to the front of
        // the Unicode extension subtag. We're then relying on ICU to follow RFC
        // 6067, which states that any trailing keywords using the same key
        // should be ignored.
        if (!intl::ApplyUnicodeExtensionToTag(cx, tag, keywords)) {
            return nullptr;
        }

        locale.clear();
        locale.encodeLatin1(cx, tag.toString(cx));
        if (!locale) {
            return nullptr;
        }
    } else {
        MOZ_ASSERT(StringsAreEqual(usage, "sort"));
    }

    // We don't need to look at the collation property - it can only be set
    // via the Unicode locale extension and is therefore already set on
    // locale.

    if (!GetProperty(cx, internals, internals, cx->names().sensitivity, &value))
        return nullptr;
    JSAutoByteString sensitivity(cx, value.toString());
    if (!sensitivity)
        return nullptr;
    if (StringsAreEqual(sensitivity, "base")) {
        uStrength = UCOL_PRIMARY;
    } else if (StringsAreEqual(sensitivity, "accent")) {
        uStrength = UCOL_SECONDARY;
    } else if (StringsAreEqual(sensitivity, "case")) {
        uStrength = UCOL_PRIMARY;
        uCaseLevel = UCOL_ON;
    } else {
        MOZ_ASSERT(StringsAreEqual(sensitivity, "variant"));
        uStrength = UCOL_TERTIARY;
    }

    if (!GetProperty(cx, internals, internals, cx->names().ignorePunctuation, &value))
        return nullptr;
    // According to the ICU team, UCOL_SHIFTED causes punctuation to be
    // ignored. Looking at Unicode Technical Report 35, Unicode Locale Data
    // Markup Language, "shifted" causes whitespace and punctuation to be
    // ignored - that's a bit more than asked for, but there's no way to get
    // less.
    if (value.toBoolean())
        uAlternate = UCOL_SHIFTED;

    if (!GetProperty(cx, internals, internals, cx->names().numeric, &value))
        return nullptr;
    if (!value.isUndefined() && value.toBoolean())
        uNumeric = UCOL_ON;

    if (!GetProperty(cx, internals, internals, cx->names().caseFirst, &value))
        return nullptr;
    if (!value.isUndefined()) {
        JSAutoByteString caseFirst(cx, value.toString());
        if (!caseFirst)
            return nullptr;
        if (StringsAreEqual(caseFirst, "upper"))
            uCaseFirst = UCOL_UPPER_FIRST;
        else if (StringsAreEqual(caseFirst, "lower"))
            uCaseFirst = UCOL_LOWER_FIRST;
        else {
            MOZ_ASSERT(StringsAreEqual(caseFirst, "false"));
            uCaseFirst = UCOL_OFF;
        }
    }

    UErrorCode status = U_ZERO_ERROR;
    UCollator* coll = ucol_open(IcuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return nullptr;
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, uStrength, &status);
    ucol_setAttribute(coll, UCOL_CASE_LEVEL, uCaseLevel, &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, uAlternate, &status);
    ucol_setAttribute(coll, UCOL_NUMERIC_COLLATION, uNumeric, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, uNormalization, &status);
    ucol_setAttribute(coll, UCOL_CASE_FIRST, uCaseFirst, &status);
    if (U_FAILURE(status)) {
        ucol_close(coll);
        intl::ReportInternalError(cx);
        return nullptr;
    }

    return coll;
}

static bool
intl_CompareStrings(JSContext* cx, UCollator* coll, HandleString str1, HandleString str2,
                    MutableHandleValue result)
{
    MOZ_ASSERT(str1);
    MOZ_ASSERT(str2);

    if (str1 == str2) {
        result.setInt32(0);
        return true;
    }

    AutoStableStringChars stableChars1(cx);
    if (!stableChars1.initTwoByte(cx, str1))
        return false;

    AutoStableStringChars stableChars2(cx);
    if (!stableChars2.initTwoByte(cx, str2))
        return false;

    mozilla::Range<const char16_t> chars1 = stableChars1.twoByteRange();
    mozilla::Range<const char16_t> chars2 = stableChars2.twoByteRange();

    UCollationResult uresult = ucol_strcoll(coll,
                                            Char16ToUChar(chars1.begin().get()), chars1.length(),
                                            Char16ToUChar(chars2.begin().get()), chars2.length());
    int32_t res;
    switch (uresult) {
        case UCOL_LESS: res = -1; break;
        case UCOL_EQUAL: res = 0; break;
        case UCOL_GREATER: res = 1; break;
        default: MOZ_CRASH("ucol_strcoll returned bad UCollationResult");
    }
    result.setInt32(res);
    return true;
}

bool
js::intl_CompareStrings(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isString());

    Rooted<CollatorObject*> collator(cx, &args[0].toObject().as<CollatorObject>());

    // Obtain a cached UCollator object.
    // XXX Does this handle Collator instances from other globals correctly?
    void* priv = collator->getReservedSlot(CollatorObject::UCOLLATOR_SLOT).toPrivate();
    UCollator* coll = static_cast<UCollator*>(priv);
    if (!coll) {
        coll = NewUCollator(cx, collator);
        if (!coll)
            return false;
        collator->setReservedSlot(CollatorObject::UCOLLATOR_SLOT, PrivateValue(coll));
    }

    // Use the UCollator to actually compare the strings.
    RootedString str1(cx, args[1].toString());
    RootedString str2(cx, args[2].toString());
    return intl_CompareStrings(cx, coll, str1, str2, args.rval());
}

bool
js::intl_isUpperCaseFirst(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    RootedString locale(cx, args[0].toString());
    bool isUpperFirst;
    if (!sharedIntlData.isUpperCaseFirst(cx, locale, &isUpperFirst))
        return false;

    args.rval().setBoolean(isUpperFirst);
    return true;
}
