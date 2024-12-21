/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.DateTimeFormat implementation. */

#include "builtin/intl/DateTimeFormat.h"

#include "mozilla/Assertions.h"
#include "mozilla/Range.h"
#include "mozilla/Span.h"

#include "jscntxt.h"
#include "jsfriendapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUHeader.h"
#include "builtin/intl/LanguageTag.h"
#include "builtin/intl/ScopedICUObject.h"
#include "builtin/intl/SharedIntlData.h"
#include "builtin/intl/TimeZoneDataGenerated.h"
#include "vm/GlobalObject.h"
#include "vm/Runtime.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::IsFinite;

using JS::ClippedTime;
using JS::TimeClip;

using js::intl::CallICU;
using js::intl::DateTimeFormatOptions;
using js::intl::IcuLocale;
using js::intl::INITIAL_CHAR_BUFFER_SIZE;
using js::intl::SharedIntlData;
using js::intl::StringsAreEqual;

/******************** DateTimeFormat ********************/

const ClassOps DateTimeFormatObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    DateTimeFormatObject::finalize
};

const Class DateTimeFormatObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DateTimeFormatObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &DateTimeFormatObject::classOps_
};

#if JS_HAS_TOSOURCE
static bool
dateTimeFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().DateTimeFormat);
    return true;
}
#endif

static const JSFunctionSpec dateTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_DateTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec dateTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_DateTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("formatToParts", "Intl_DateTimeFormat_formatToParts", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, dateTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 12.2.1 Intl.DateTimeFormat([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
DateTimeFormat(JSContext* cx, const CallArgs& args, bool construct, DateTimeFormatOptions dtfOptions)
{
    // Step 1 (Handled by OrdinaryCreateFromConstructor fallback code).

    // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
    RootedObject proto(cx);
    if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    if (!proto) {
        proto = GlobalObject::getOrCreateDateTimeFormatPrototype(cx, cx->global());
        if (!proto)
            return false;
    }

    Rooted<DateTimeFormatObject*> dateTimeFormat(cx);
    dateTimeFormat = NewObjectWithGivenProto<DateTimeFormatObject>(cx, proto);
    if (!dateTimeFormat)
        return false;

    dateTimeFormat->setReservedSlot(DateTimeFormatObject::INTERNALS_SLOT, NullValue());
    dateTimeFormat->setReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT,
                                    PrivateValue(nullptr));

    RootedValue thisValue(cx, construct ? ObjectValue(*dateTimeFormat) : args.thisv());
    RootedValue locales(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    // Step 3.
    return intl::LegacyIntlInitialize(cx, dateTimeFormat, cx->names().InitializeDateTimeFormat,
                                      thisValue, locales, options, dtfOptions, args.rval());
}

static bool
DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return DateTimeFormat(cx, args, args.isConstructing(), DateTimeFormatOptions::Standard);
}

static bool
MozDateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Don't allow to call mozIntl.DateTimeFormat as a function. That way we
    // don't need to worry how to handle the legacy initialization semantics
    // when applied on mozIntl.DateTimeFormat.
    if (!ThrowIfNotConstructing(cx, args, "mozIntl.DateTimeFormat"))
        return false;

    return DateTimeFormat(cx, args, true, DateTimeFormatOptions::EnableMozExtensions);
}

bool
js::intl_DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_DateTimeFormat is an intrinsic for self-hosted JavaScript, so it
    // cannot be used with "new", but it still has to be treated as a
    // constructor.
    return DateTimeFormat(cx, args, true, DateTimeFormatOptions::Standard);
}

void
js::DateTimeFormatObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    const Value& slot = obj->as<DateTimeFormatObject>().getReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT);
    if (UDateFormat* df = static_cast<UDateFormat*>(slot.toPrivate()))
        udat_close(df);
}

JSObject*
js::CreateDateTimeFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global,
                                  MutableHandleObject constructor, DateTimeFormatOptions dtfOptions)
{
    RootedFunction ctor(cx);
    ctor = dtfOptions == DateTimeFormatOptions::EnableMozExtensions
           ? GlobalObject::createConstructor(cx, MozDateTimeFormat, cx->names().DateTimeFormat, 0)
           : GlobalObject::createConstructor(cx, DateTimeFormat, cx->names().DateTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedObject proto(cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
    if (!proto)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 12.2.2
    if (!JS_DefineFunctions(cx, ctor, dateTimeFormat_static_methods))
        return nullptr;

    // 12.3.2 and 12.3.3
    if (!JS_DefineFunctions(cx, proto, dateTimeFormat_methods))
        return nullptr;

    // Install a getter for DateTimeFormat.prototype.format that returns a
    // formatting function bound to a specified DateTimeFormat object (suitable
    // for passing to methods like Array.prototype.map).
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().DateTimeFormatFormatGet,
                                         &getter))
    {
        return nullptr;
    }
    if (!DefineProperty(cx, proto, cx->names().format, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().DateTimeFormat, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    constructor.set(ctor);
    return proto;
}

bool
js::AddMozDateTimeFormatConstructor(JSContext* cx, JS::Handle<JSObject*> intl)
{
    Handle<GlobalObject*> global = cx->global();

    RootedObject mozDateTimeFormat(cx);
    JSObject* mozDateTimeFormatProto =
        CreateDateTimeFormatPrototype(cx, intl, global, &mozDateTimeFormat, DateTimeFormatOptions::EnableMozExtensions);
    return mozDateTimeFormatProto != nullptr;
}

static bool
DefaultCalendar(JSContext* cx, const JSAutoByteString& locale, MutableHandleValue rval)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar* cal = ucal_open(nullptr, 0, locale.ptr(), UCAL_DEFAULT, &status);

    // This correctly handles nullptr |cal| when opening failed.
    ScopedICUObject<UCalendar, ucal_close> closeCalendar(cal);

    const char* calendar = ucal_getType(cal, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    // ICU returns old-style keyword values; map them to BCP 47 equivalents
    JSString* str = JS_NewStringCopyZ(cx, uloc_toUnicodeLocaleType("ca", calendar));
    if (!str)
        return false;

    rval.setString(str);
    return true;
}

struct CalendarAlias
{
    const char* const calendar;
    const char* const alias;
};

const CalendarAlias calendarAliases[] = {
    { "islamic-civil", "islamicc" },
    { "ethioaa", "ethiopic-amete-alem" }
};

bool
js::intl_availableCalendars(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    RootedObject calendars(cx, NewDenseEmptyArray(cx));
    if (!calendars)
        return false;
    uint32_t index = 0;

    // We need the default calendar for the locale as the first result.
    RootedValue element(cx);
    if (!DefaultCalendar(cx, locale, &element))
        return false;

    if (!DefineElement(cx, calendars, index++, element))
        return false;

    // Now get the calendars that "would make a difference", i.e., not the default.
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucal_getKeywordValuesForLocale("ca", locale.ptr(), false, &status);
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

    for (; count > 0; count--) {
        const char* calendar = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            intl::ReportInternalError(cx);
            return false;
        }

        // ICU returns old-style keyword values; map them to BCP 47 equivalents
        calendar = uloc_toUnicodeLocaleType("ca", calendar);

        JSString* jscalendar = JS_NewStringCopyZ(cx, calendar);
        if (!jscalendar)
            return false;
        element = StringValue(jscalendar);
        if (!DefineElement(cx, calendars, index++, element))
            return false;

        // ICU doesn't return calendar aliases, append them here.
        for (const auto& calendarAlias : calendarAliases) {
            if (StringsAreEqual(calendar, calendarAlias.calendar)) {
                JSString* jscalendar = JS_NewStringCopyZ(cx, calendarAlias.alias);
                if (!jscalendar)
                    return false;
                element = StringValue(jscalendar);
                if (!DefineElement(cx, calendars, index++, element))
                    return false;
            }
        }
    }

    args.rval().setObject(*calendars);
    return true;
}

bool
js::intl_defaultCalendar(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    return DefaultCalendar(cx, locale, args.rval());
}

bool
js::intl_IsValidTimeZoneName(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    RootedString timeZone(cx, args[0].toString());
    RootedString validatedTimeZone(cx);
    if (!sharedIntlData.validateTimeZoneName(cx, timeZone, &validatedTimeZone))
        return false;

    if (validatedTimeZone)
        args.rval().setString(validatedTimeZone);
    else
        args.rval().setNull();

    return true;
}

bool
js::intl_canonicalizeTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    // Some time zone names are canonicalized differently by ICU -- handle
    // those first:
    RootedString timeZone(cx, args[0].toString());
    RootedString ianaTimeZone(cx);
    if (!sharedIntlData.tryCanonicalizeTimeZoneConsistentWithIANA(cx, timeZone, &ianaTimeZone))
        return false;

    if (ianaTimeZone) {
        args.rval().setString(ianaTimeZone);
        return true;
    }

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, timeZone))
        return false;

    mozilla::Range<const char16_t> tzchars = stableChars.twoByteRange();

    JSString* str = CallICU(cx, [&tzchars](UChar* chars, uint32_t size, UErrorCode* status) {
        return ucal_getCanonicalTimeZoneID(tzchars.begin().get(), tzchars.length(),
                                           chars, size, nullptr, status);
    });
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    // The current default might be stale, because JS::ResetTimeZone() doesn't
    // immediately update ICU's default time zone. So perform an update if
    // needed.
    js::ResyncICUDefaultTimeZone();

    JSString* str = CallICU(cx, ucal_getDefaultTimeZone);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZoneOffset(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    UErrorCode status = U_ZERO_ERROR;
    const UChar* uTimeZone = nullptr;
    int32_t uTimeZoneLength = 0;
    const char* rootLocale = "";
    UCalendar* cal = ucal_open(uTimeZone, uTimeZoneLength, rootLocale, UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UCalendar, ucal_close> toClose(cal);

    int32_t offset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    args.rval().setInt32(offset);
    return true;
}

enum class HourCycle {
    // 12 hour cycle, from 0 to 11.
    H11,

    // 12 hour cycle, from 1 to 12.
    H12,

    // 24 hour cycle, from 0 to 23.
    H23,

    // 24 hour cycle, from 1 to 24.
    H24
};

static bool
IsHour12(HourCycle hc)
{
    return hc == HourCycle::H11 || hc == HourCycle::H12;
}

static char16_t
HourSymbol(HourCycle hc)
{
    switch (hc) {
      case HourCycle::H11:
        return 'K';
      case HourCycle::H12:
        return 'h';
      case HourCycle::H23:
        return 'H';
      case HourCycle::H24:
        return 'k';
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected hour cycle");
}

/**
* Parse a pattern according to the format specified in
* <https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns>.
*/
template <typename CharT>
class PatternIterator {
    CharT* iter_;
    const CharT* const end_;

  public:
    explicit PatternIterator(mozilla::Span<CharT> pattern)
      : iter_(pattern.data()), end_(pattern.data() + pattern.size()) {}

    CharT* next() {
        MOZ_ASSERT(iter_ != nullptr);

        bool inQuote = false;
        while (iter_ < end_) {
            CharT* cur = iter_++;
            if (*cur == '\'') {
                inQuote = !inQuote;
            } else if (!inQuote) {
                return cur;
            }
        }

        iter_ = nullptr;
        return nullptr;
    }
};

/**
* Return the hour cycle for the given option string.
*/
static HourCycle
HourCycleFromOption(JSLinearString* str)
{
    if (StringEqualsAscii(str, "h11")) {
        return HourCycle::H11;
    }
    if (StringEqualsAscii(str, "h12")) {
        return HourCycle::H12;
    }
    if (StringEqualsAscii(str, "h23")) {
        return HourCycle::H23;
    }
    MOZ_ASSERT(StringEqualsAscii(str, "h24"));
    return HourCycle::H24;
}

/**
* Return the hour cycle used in the input pattern or Nothing if none was found.
*/
static mozilla::Maybe<HourCycle>
HourCycleFromPattern(mozilla::Span<const char16_t> pattern)
{
    PatternIterator<const char16_t> iter(pattern);
    while (const auto* ptr = iter.next()) {
        switch (*ptr) {
          case 'K':
            return mozilla::Some(HourCycle::H11);
          case 'h':
            return mozilla::Some(HourCycle::H12);
          case 'H':
            return mozilla::Some(HourCycle::H23);
          case 'k':
            return mozilla::Some(HourCycle::H24);
        }
    }
    return mozilla::Nothing();
}

/**
* Replaces all hour pattern characters in |pattern| to use the matching hour
* representation for |hourCycle|.
*/
static void
ReplaceHourSymbol(mozilla::Span<char16_t> pattern, HourCycle hc)
{
    char16_t replacement = HourSymbol(hc);
    PatternIterator<char16_t> iter(pattern);
    while (auto* ptr = iter.next()) {
        char16_t ch = *ptr;
        if (ch == 'K' || ch == 'h' || ch == 'H' || ch == 'k') {
            *ptr = replacement;
        }
    }
}

bool
js::intl_patternForSkeleton(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isString() || args[2].isUndefined());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    JSFlatString* skeletonFlat = args[1].toString()->ensureFlat(cx);
    if (!skeletonFlat)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, skeletonFlat))
        return false;

    mozilla::Maybe<HourCycle> hourCycle;
    if (args[2].isString()) {
        JSLinearString* hourCycleStr = args[2].toString()->ensureLinear(cx);
        if (!hourCycleStr) {
            return false;
        }

        hourCycle.emplace(HourCycleFromOption(hourCycleStr));
    }

    mozilla::Range<const char16_t> skeletonChars = stableChars.twoByteRange();
    uint32_t skeletonLen = u_strlen(Char16ToUChar(skeletonChars.begin().get()));

    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* gen = udatpg_open(IcuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UDateTimePatternGenerator, udatpg_close> toClose(gen);

    Vector<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> pattern(cx);

    int32_t patternSize = CallICU(
        cx,
        pattern,
        [gen, &skeletonChars](UChar* chars, uint32_t size, UErrorCode* status) {
            return udatpg_getBestPattern(gen, skeletonChars.begin().get(),
            skeletonChars.length(), chars, size, status);
        });
    if (patternSize < 0) {
        return false;
    }

    // If the hourCycle option was set, adjust the resolved pattern to use the
    // requested hour cycle representation.
    if (hourCycle) {
        ReplaceHourSymbol(pattern, hourCycle.value());
    }

    JSString* str = NewStringCopyN<CanGC>(cx, pattern.begin(), pattern.length());
    if (!str) {
        return false;
    }
    args.rval().setString(str);
    return true;
}

/**
 * Find a matching pattern using the requested hour-12 options.
 *
 * This function is needed to work around the following two issues.
 * - https://unicode-org.atlassian.net/browse/ICU-21023
 * - https://unicode-org.atlassian.net/browse/CLDR-13425
 *
 * We're currently using a relatively simple workaround, which doesn't give the
 * most accurate results. For example:
 *
 * ```
 * var dtf = new Intl.DateTimeFormat("en", {
 *   timeZone: "UTC",
 *   dateStyle: "long",
 *   timeStyle: "long",
 *   hourCycle: "h12",
 * });
 * print(dtf.format(new Date("2020-01-01T00:00Z")));
 * ```
 *
 * Returns the pattern "MMMM d, y 'at' h:mm:ss a z", but when going through
 * |udatpg_getSkeleton| and then |udatpg_getBestPattern| to find an equivalent
 * pattern for "h23", we'll end up with the pattern "MMMM d, y, HH:mm:ss z", so
 * the combinator element " 'at' " was lost in the process.
 */
template <size_t N>
static bool
FindPatternWithHourCycle(JSContext* cx, const char* locale,
                         Vector<char16_t, N>& pattern, bool hour12)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* gen = udatpg_open(IcuLocale(locale), &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UDateTimePatternGenerator, udatpg_close> toClose(gen);

    if (!gen) {
        return false;
    }

    Vector<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> skeleton(cx);

    int32_t skeletonSize = CallICU(
        cx,
        skeleton,
        [&pattern](UChar* chars, uint32_t size, UErrorCode* status) {
            return udatpg_getSkeleton(nullptr, pattern.begin(), pattern.length(),
            chars, size, status);
        });
    if (skeletonSize < 0) {
        return false;
    }

    // Input skeletons don't differentiate between "K" and "h" resp. "k" and "H".
    ReplaceHourSymbol(skeleton, hour12 ? HourCycle::H12 : HourCycle::H23);

    MOZ_ALWAYS_TRUE(pattern.resize(0));

    int32_t patternSize = CallICU(
        cx,
        pattern,
        [gen, &skeleton](UChar* chars, uint32_t size, UErrorCode* status) {
            return udatpg_getBestPattern(gen, skeleton.begin(), skeleton.length(),
            chars, size, status);
        });
    if (patternSize < 0) {
        return false;
    }

    return true;
}

bool
js::intl_patternForStyle(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 6);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString() || args[1].isUndefined());
    MOZ_ASSERT(args[2].isString() || args[2].isUndefined());
    MOZ_ASSERT(args[3].isString());
    MOZ_ASSERT(args[4].isBoolean() || args[4].isUndefined());
    MOZ_ASSERT(args[5].isString() || args[5].isUndefined());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    auto toDateFormatStyle = [](JSLinearString* str) {
        if (StringEqualsAscii(str, "full")) {
            return UDAT_FULL;
        }
        if (StringEqualsAscii(str, "long")) {
            return UDAT_LONG;
        }
        if (StringEqualsAscii(str, "medium")) {
            return UDAT_MEDIUM;
        }
        MOZ_ASSERT(StringEqualsAscii(str, "short"));
        return UDAT_SHORT;
    };

    UDateFormatStyle dateStyle = UDAT_NONE;

    if (args[1].isString()) {
        JSLinearString* dateStyleStr = args[1].toString()->ensureLinear(cx);
        if (!dateStyleStr)
            return false;

        dateStyle = toDateFormatStyle(dateStyleStr);
    }

    UDateFormatStyle timeStyle = UDAT_NONE;
    if (args[2].isString()) {
        JSLinearString* timeStyleStr = args[2].toString()->ensureLinear(cx);
        if (!timeStyleStr)
            return false;

        timeStyle = toDateFormatStyle(timeStyleStr);
    }

    AutoStableStringChars timeZone(cx);
    if (!timeZone.initTwoByte(cx, args[3].toString()))
        return false;

    mozilla::Maybe<bool> hour12;
    if (args[4].isBoolean()) {
        hour12.emplace(args[4].toBoolean());
    }

    mozilla::Maybe<HourCycle> hourCycle;
    if (args[5].isString()) {
        JSLinearString* hourCycleStr = args[5].toString()->ensureLinear(cx);
        if (!hourCycleStr) {
            return false;
        }

        hourCycle.emplace(HourCycleFromOption(hourCycleStr));
    }

    mozilla::Range<const char16_t> timeZoneChars = timeZone.twoByteRange();

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat* df = udat_open(timeStyle, dateStyle, IcuLocale(locale.ptr()),
                                Char16ToUChar(timeZoneChars.begin().get()),
                                timeZoneChars.length(), nullptr, -1, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UDateFormat, udat_close> toClose(df);

    Vector<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> pattern(cx);

    int32_t patternSize = CallICU(
        cx,
        pattern,
        [df](UChar* chars, uint32_t size, UErrorCode* status) {
            return udat_toPattern(df, false, chars, size, status);
        });
    if (patternSize < 0) {
        return false;
    }

    // If a specific hour cycle was requested and this hour cycle doesn't match
    // the hour cycle used in the resolved pattern, find an equivalent pattern
    // with the correct hour cycle.
    if (timeStyle != UDAT_NONE && (hour12 || hourCycle)) {
        if (auto hcPattern = HourCycleFromPattern(pattern)) {
            bool wantHour12 = hour12 ? hour12.value() : IsHour12(hourCycle.value());
            if (wantHour12 != IsHour12(hcPattern.value())) {
                if (!FindPatternWithHourCycle(cx, locale.ptr(), pattern, wantHour12)) {
                    return false;
                }
            }
        }
    }

    // If the hourCycle option was set, adjust the resolved pattern to use the
    // requested hour cycle representation.
    if (hourCycle) {
        ReplaceHourSymbol(pattern, hourCycle.value());
    }

    JSString* str = NewStringCopyN<CanGC>(cx, pattern.begin(), pattern.length());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/**
 * Returns a new UDateFormat with the locale and date-time formatting options
 * of the given DateTimeFormat.
 */
static UDateFormat*
NewUDateFormat(JSContext* cx, Handle<DateTimeFormatObject*> dateTimeFormat)
{
    RootedValue value(cx);

    RootedObject internals(cx, intl::GetInternalsObject(cx, dateTimeFormat));
    if (!internals)
       return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;

    // ICU expects calendar and numberingSystem as Unicode locale extensions on
    // locale.

    intl::LanguageTag tag(cx);
    {
        JSLinearString* locale = value.toString()->ensureLinear(cx);
        if (!locale)
            return nullptr;

        if (!intl::LanguageTagParser::parse(cx, locale, tag))
            return nullptr;
    }

    JS::RootedVector<intl::UnicodeExtensionKeyword> keywords(cx);

    if (!GetProperty(cx, internals, internals, cx->names().calendar, &value))
        return nullptr;

    {
        JSLinearString* calendar = value.toString()->ensureLinear(cx);
        if (!calendar)
            return nullptr;

        if (!keywords.emplaceBack("ca", calendar))
            return nullptr;
    }

    if (!GetProperty(cx, internals, internals, cx->names().numberingSystem, &value))
        return nullptr;

    {
        JSLinearString* numberingSystem = value.toString()->ensureLinear(cx);
        if (!numberingSystem)
            return nullptr;

        if (!keywords.emplaceBack("nu", numberingSystem))
            return nullptr;
    }

    // |ApplyUnicodeExtensionToTag| applies the new keywords to the front of
    // the Unicode extension subtag. We're then relying on ICU to follow RFC
    // 6067, which states that any trailing keywords using the same key
    // should be ignored.
    if (!intl::ApplyUnicodeExtensionToTag(cx, tag, keywords))
        return nullptr;

    UniqueChars locale = tag.toStringZ(cx);
    if (!locale)
        return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().timeZone, &value))
        return nullptr;

    AutoStableStringChars timeZoneChars(cx);
    Rooted<JSFlatString*> timeZoneFlat(cx, value.toString()->ensureFlat(cx));
    if (!timeZoneFlat || !timeZoneChars.initTwoByte(cx, timeZoneFlat))
        return nullptr;

    const UChar* uTimeZone = Char16ToUChar(timeZoneChars.twoByteRange().begin().get());
    uint32_t uTimeZoneLength = u_strlen(uTimeZone);

    if (!GetProperty(cx, internals, internals, cx->names().pattern, &value))
        return nullptr;

    AutoStableStringChars patternChars(cx);
    Rooted<JSFlatString*> patternFlat(cx, value.toString()->ensureFlat(cx));
    if (!patternFlat || !patternChars.initTwoByte(cx, patternFlat))
        return nullptr;

    const UChar* uPattern = Char16ToUChar(patternChars.twoByteRange().begin().get());
    uint32_t uPatternLength = u_strlen(uPattern);

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat* df =
        udat_open(UDAT_PATTERN, UDAT_PATTERN, IcuLocale(locale.get()), uTimeZone, uTimeZoneLength,
                  uPattern, uPatternLength, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return nullptr;
    }

    // ECMAScript requires the Gregorian calendar to be used from the beginning
    // of ECMAScript time.
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(df));
    ucal_setGregorianChange(cal, StartOfTime, &status);

    // An error here means the calendar is not Gregorian, so we don't care.

    return df;
}

static bool
intl_FormatDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE,
                                  "DateTimeFormat", "format");
        return false;
    }

    JSString* str = CallICU(cx, [df, x](UChar* chars, int32_t size, UErrorCode* status) {
        return udat_format(df, x, chars, size, nullptr, status);
    });
    if (!str)
        return false;

    result.setString(str);
    return true;
}

using FieldType = ImmutablePropertyNamePtr JSAtomState::*;

static FieldType
GetFieldTypeForFormatField(UDateFormatField fieldName)
{
    // See intl/icu/source/i18n/unicode/udat.h for a detailed field list.  This
    // switch is deliberately exhaustive: cases might have to be added/removed
    // if this code is compiled with a different ICU with more
    // UDateFormatField enum initializers.  Please guard such cases with
    // appropriate ICU version-testing #ifdefs, should cross-version divergence
    // occur.
    switch (fieldName) {
      case UDAT_ERA_FIELD:
        return &JSAtomState::era;
      case UDAT_YEAR_FIELD:
      case UDAT_YEAR_WOY_FIELD:
      case UDAT_EXTENDED_YEAR_FIELD:
      case UDAT_YEAR_NAME_FIELD:
        return &JSAtomState::year;

      case UDAT_MONTH_FIELD:
      case UDAT_STANDALONE_MONTH_FIELD:
        return &JSAtomState::month;

      case UDAT_DATE_FIELD:
      case UDAT_JULIAN_DAY_FIELD:
        return &JSAtomState::day;

      case UDAT_HOUR_OF_DAY1_FIELD:
      case UDAT_HOUR_OF_DAY0_FIELD:
      case UDAT_HOUR1_FIELD:
      case UDAT_HOUR0_FIELD:
        return &JSAtomState::hour;

      case UDAT_MINUTE_FIELD:
        return &JSAtomState::minute;

      case UDAT_SECOND_FIELD:
        return &JSAtomState::second;

      case UDAT_DAY_OF_WEEK_FIELD:
      case UDAT_STANDALONE_DAY_FIELD:
      case UDAT_DOW_LOCAL_FIELD:
      case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
        return &JSAtomState::weekday;

      case UDAT_AM_PM_FIELD:
        return &JSAtomState::dayPeriod;

      case UDAT_TIMEZONE_FIELD:
        return &JSAtomState::timeZoneName;

      case UDAT_FRACTIONAL_SECOND_FIELD:
      case UDAT_DAY_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_MONTH_FIELD:
      case UDAT_MILLISECONDS_IN_DAY_FIELD:
      case UDAT_TIMEZONE_RFC_FIELD:
      case UDAT_TIMEZONE_GENERIC_FIELD:
      case UDAT_QUARTER_FIELD:
      case UDAT_STANDALONE_QUARTER_FIELD:
      case UDAT_TIMEZONE_SPECIAL_FIELD:
      case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
      case UDAT_TIMEZONE_ISO_FIELD:
      case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
#ifndef U_HIDE_INTERNAL_API
      case UDAT_RELATED_YEAR_FIELD:
#endif
#ifndef U_HIDE_DRAFT_API
      case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
      case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
#endif
#ifndef U_HIDE_INTERNAL_API
      case UDAT_TIME_SEPARATOR_FIELD:
#endif
        // These fields are all unsupported.
        return nullptr;

      case UDAT_FIELD_COUNT:
        MOZ_ASSERT_UNREACHABLE("format field sentinel value returned by "
                               "iterator!");
    }

    MOZ_ASSERT_UNREACHABLE("unenumerated, undocumented format field returned "
                           "by iterator");
    return nullptr;
}

static bool
intl_FormatToPartsDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE,
                                  "DateTimeFormat", "formatToParts");
        return false;
    }

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    UErrorCode status = U_ZERO_ERROR;
    UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }
    ScopedICUObject<UFieldPositionIterator, ufieldpositer_close> toClose(fpositer);

    RootedString overallResult(cx);
    overallResult = CallICU(cx, [df, x, fpositer](UChar* chars, int32_t size, UErrorCode* status) {
        return udat_formatForFields(df, x, chars, size, fpositer, status);
    });
    if (!overallResult)
        return false;

    RootedArrayObject partsArray(cx, NewDenseEmptyArray(cx));
    if (!partsArray)
        return false;
    if (overallResult->length() == 0) {
        // An empty string contains no parts, so avoid extra work below.
        result.setObject(*partsArray);
        return true;
    }

    size_t lastEndIndex = 0;

    uint32_t partIndex = 0;
    RootedObject singlePart(cx);
    RootedValue partType(cx);
    RootedValue val(cx);

    auto AppendPart = [&](FieldType type, size_t beginIndex, size_t endIndex) {
        singlePart = NewBuiltinClassInstance<PlainObject>(cx);
        if (!singlePart)
            return false;

        partType = StringValue(cx->names().*type);
        if (!DefineProperty(cx, singlePart, cx->names().type, partType))
            return false;

        JSLinearString* partSubstr =
            NewDependentString(cx, overallResult, beginIndex, endIndex - beginIndex);
        if (!partSubstr)
            return false;

        val = StringValue(partSubstr);
        if (!DefineProperty(cx, singlePart, cx->names().value, val))
            return false;

        val = ObjectValue(*singlePart);
        if (!DefineElement(cx, partsArray, partIndex, val))
            return false;

        lastEndIndex = endIndex;
        partIndex++;
        return true;
    };

    int32_t fieldInt, beginIndexInt, endIndexInt;
    while ((fieldInt = ufieldpositer_next(fpositer, &beginIndexInt, &endIndexInt)) >= 0) {
        MOZ_ASSERT(beginIndexInt >= 0);
        MOZ_ASSERT(endIndexInt >= 0);
        MOZ_ASSERT(beginIndexInt <= endIndexInt,
                   "field iterator returning invalid range");

        size_t beginIndex(beginIndexInt);
        size_t endIndex(endIndexInt);

        // Technically this isn't guaranteed.  But it appears true in pratice,
        // and http://bugs.icu-project.org/trac/ticket/12024 is expected to
        // correct the documentation lapse.
        MOZ_ASSERT(lastEndIndex <= beginIndex,
                   "field iteration didn't return fields in order start to "
                   "finish as expected");

        if (FieldType type = GetFieldTypeForFormatField(static_cast<UDateFormatField>(fieldInt))) {
            if (lastEndIndex < beginIndex) {
                if (!AppendPart(&JSAtomState::literal, lastEndIndex, beginIndex))
                    return false;
            }

            if (!AppendPart(type, beginIndex, endIndex))
                return false;
        }
    }

    // Append any final literal.
    if (lastEndIndex < overallResult->length()) {
        if (!AppendPart(&JSAtomState::literal, lastEndIndex, overallResult->length()))
            return false;
    }

    result.setObject(*partsArray);
    return true;
}

bool
js::intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isNumber());
    MOZ_ASSERT(args[2].isBoolean());

    Rooted<DateTimeFormatObject*> dateTimeFormat(cx);
    dateTimeFormat = &args[0].toObject().as<DateTimeFormatObject>();

    // Obtain a cached UDateFormat object.
    void* priv =
        dateTimeFormat->getReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT).toPrivate();
    UDateFormat* df = static_cast<UDateFormat*>(priv);
    if (!df) {
        df = NewUDateFormat(cx, dateTimeFormat);
        if (!df)
            return false;
        dateTimeFormat->setReservedSlot(DateTimeFormatObject::UDATE_FORMAT_SLOT, PrivateValue(df));
    }

    // Use the UDateFormat to actually format the time stamp.
    return args[2].toBoolean()
           ? intl_FormatToPartsDateTime(cx, df, args[1].toNumber(), args.rval())
           : intl_FormatDateTime(cx, df, args[1].toNumber(), args.rval());
}

