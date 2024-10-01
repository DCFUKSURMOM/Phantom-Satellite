/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/********** Intl.DateTimeFormat **********/


/**
 * Compute an internal properties object from |lazyDateTimeFormatData|.
 */
function resolveDateTimeFormatInternals(lazyDateTimeFormatData) {
    assert(IsObject(lazyDateTimeFormatData), "lazy data not an object?");

    // Lazy DateTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //
    //     localeOpt: // *first* opt computed in InitializeDateTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //
    //         ca: string matching a Unicode extension type, // optional
    //
    //         nu: string matching a Unicode extension type, // optional
    //
    //         hc: "h11" / "h12" / "h23" / "h24", // optional
    //       }
    //
    //     timeZone: IANA time zone name,
    //
    //     formatOpt: // *second* opt computed in InitializeDateTimeFormat
    //       {
    //         // all the properties/values listed in Table 3
    //         // (weekday, era, year, month, day, &c.)
    //
    //         hour12: true / false,  // optional
    //       }
    //
    //     formatMatcher: "basic" / "best fit",
    //
    //     dateStyle: "full" / "long" / "medium" / "short" / undefined,
    //
    //     timeStyle: "full" / "long" / "medium" / "short" / undefined,
    //
    //     patternOption:
    //       String representing LDML Date Format pattern or undefined
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every DateTimeFormat lazy data object has *all* these properties,
    // never a subset of them.

    var internalProps = std_Object_create(null);
    
    var DateTimeFormat = dateTimeFormatInternalProperties;

    // Compute effective locale.

    // Step 10.
    var localeData = DateTimeFormat.localeData;

    // Step 11.
    var r = ResolveLocale("DateTimeFormat",
                          lazyDateTimeFormatData.requestedLocales,
                          lazyDateTimeFormatData.localeOpt,
                          DateTimeFormat.relevantExtensionKeys,
                          localeData);

    // Steps 12-13, 15.
    internalProps.locale = r.locale;
    internalProps.calendar = r.ca;
    internalProps.numberingSystem = r.nu;

    // Compute formatting options.
    // Step 14.
    var dataLocale = r.dataLocale;

    // Steps 20.
    internalProps.timeZone = lazyDateTimeFormatData.timeZone;

    // Step 21.
    var formatOpt = lazyDateTimeFormatData.formatOpt;

    // Step 16.
    // Copy the hourCycle setting, if present, to the format options. But
    // only do this if no hour12 option is present, because the latter takes
    // precedence over hourCycle.
    if (r.hc !== null && formatOpt.hour12 === undefined)
        formatOpt.hourCycle = r.hc;

    // Steps 26-30, more or less - see comment after this function.
    var pattern;
    if (lazyDateTimeFormatData.patternOption !== undefined) {
        pattern = lazyDateTimeFormatData.patternOption;

        internalProps.patternOption = lazyDateTimeFormatData.patternOption;
    } else if (lazyDateTimeFormatData.dateStyle !== undefined ||
               lazyDateTimeFormatData.timeStyle !== undefined) {
        pattern = intl_patternForStyle(dataLocale,
                                       lazyDateTimeFormatData.dateStyle,
                                       lazyDateTimeFormatData.timeStyle,
                                       lazyDateTimeFormatData.timeZone,
                                       formatOpt.hour12,
                                       formatOpt.hourCycle);

        internalProps.dateStyle = lazyDateTimeFormatData.dateStyle;
        internalProps.timeStyle = lazyDateTimeFormatData.timeStyle;
    } else {
        pattern = toBestICUPattern(dataLocale, formatOpt);
    }

    // Step 31.
    internalProps.pattern = pattern;

    // The caller is responsible for associating |internalProps| with the right
    // object using |setInternalProperties|.
    return internalProps;
}


/**
 * Returns an object containing the DateTimeFormat internal properties of |obj|.
 */
function getDateTimeFormatInternals(obj) {
    assert(IsObject(obj), "getDateTimeFormatInternals called with non-object");
    assert(IsDateTimeFormat(obj), "getDateTimeFormatInternals called with non-DateTimeFormat");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "DateTimeFormat", "bad type escaped getIntlObjectInternals");

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    internalProps = resolveDateTimeFormatInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}

/**
 * Components of date and time formats and their values.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
var dateTimeComponentValues = {
    weekday: ["narrow", "short", "long"],
    era: ["narrow", "short", "long"],
    year: ["2-digit", "numeric"],
    month: ["2-digit", "numeric", "narrow", "short", "long"],
    day: ["2-digit", "numeric"],
    hour: ["2-digit", "numeric"],
    minute: ["2-digit", "numeric"],
    second: ["2-digit", "numeric"],
    timeZoneName: ["short", "long"]
};


var dateTimeComponents = std_Object_getOwnPropertyNames(dateTimeComponentValues);

var timeZoneCache = {
    icuDefaultTimeZone: undefined,
    defaultTimeZone: undefined,
};


/**
 * 6.4.2 CanonicalizeTimeZoneName ( timeZone )
 *
 * Canonicalizes the given IANA time zone name.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 */
function CanonicalizeTimeZoneName(timeZone) {
    assert(typeof timeZone === "string", "CanonicalizeTimeZoneName");

    // Step 1. (Not applicable, the input is already a valid IANA time zone.)
    assert(timeZone !== "Etc/Unknown", "Invalid time zone");
    assert(timeZone === intl_IsValidTimeZoneName(timeZone), "Time zone name not normalized");

    // Step 2.
    var ianaTimeZone = intl_canonicalizeTimeZone(timeZone);
    assert(ianaTimeZone !== "Etc/Unknown", "Invalid canonical time zone");
    assert(ianaTimeZone === intl_IsValidTimeZoneName(ianaTimeZone), "Unsupported canonical time zone");

    // Step 3.
    if (ianaTimeZone === "Etc/UTC" || ianaTimeZone === "Etc/GMT") {
        // ICU/CLDR canonicalizes Etc/UCT to Etc/GMT, but following IANA and
        // ECMA-402 to the letter means Etc/UCT is a separate time zone.
        if (timeZone === "Etc/UCT" || timeZone === "UCT")
            ianaTimeZone = "Etc/UCT";
        else
            ianaTimeZone = "UTC";
    }

    // Step 4.
    return ianaTimeZone;
}


/**
 * 6.4.3 DefaultTimeZone ()
 *
 * Returns the IANA time zone name for the host environment's current time zone.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 */
function DefaultTimeZone() {
    const icuDefaultTimeZone = intl_defaultTimeZone();
    if (timeZoneCache.icuDefaultTimeZone === icuDefaultTimeZone)
        return timeZoneCache.defaultTimeZone;

    // Verify that the current ICU time zone is a valid ECMA-402 time zone.
    var timeZone = intl_IsValidTimeZoneName(icuDefaultTimeZone);
    if (timeZone === null) {
        // Before defaulting to "UTC", try to represent the default time zone
        // using the Etc/GMT + offset format. This format only accepts full
        // hour offsets.
        const msPerHour = 60 * 60 * 1000;
        var offset = intl_defaultTimeZoneOffset();
        assert(offset === (offset | 0),
               "milliseconds offset shouldn't be able to exceed int32_t range");
        var offsetHours = offset / msPerHour, offsetHoursFraction = offset % msPerHour;
        if (offsetHoursFraction === 0) {
            // Etc/GMT + offset uses POSIX-style signs, i.e. a positive offset
            // means a location west of GMT.
            timeZone = "Etc/GMT" + (offsetHours < 0 ? "+" : "-") + std_Math_abs(offsetHours);

            // Check if the fallback is valid.
            timeZone = intl_IsValidTimeZoneName(timeZone);
        }

        // Fallback to "UTC" if everything else fails.
        if (timeZone === null)
            timeZone = "UTC";
    }

    // Canonicalize the ICU time zone, e.g. change Etc/UTC to UTC.
    var defaultTimeZone = CanonicalizeTimeZoneName(timeZone);

    timeZoneCache.defaultTimeZone = defaultTimeZone;
    timeZoneCache.icuDefaultTimeZone = icuDefaultTimeZone;

    return defaultTimeZone;
}


/**
 * 12.1.10 UnwrapDateTimeFormat( dtf )
 */
function UnwrapDateTimeFormat(dtf, methodName) {
    // Step 1 (not applicable in our implementation).

    // Step 2.
    if ((!IsObject(dtf) || !IsDateTimeFormat(dtf)) &&
        dtf instanceof GetDateTimeFormatConstructor())
    {
        dtf = dtf[intlFallbackSymbol()];
    }

    // Step 3.
    if (!IsObject(dtf) || !IsDateTimeFormat(dtf)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "DateTimeFormat", methodName,
                       "DateTimeFormat");
    }

    // Step 4.
    return dtf;
}


/**
 * Initializes an object as a DateTimeFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a DateTimeFormat.
 * This later work occurs in |resolveDateTimeFormatInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function InitializeDateTimeFormat(dateTimeFormat, thisValue, locales, options, mozExtensions) {
    assert(IsObject(dateTimeFormat), "InitializeDateTimeFormat called with non-Object");
    assert(IsDateTimeFormat(dateTimeFormat),
           "InitializeDateTimeFormat called with non-DateTimeFormat");

    // Lazy DateTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //
    //     localeOpt: // *first* opt computed in InitializeDateTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //
    //         ca: string matching a Unicode extension type, // optional
    //
    //         nu: string matching a Unicode extension type, // optional
    //
    //         hc: "h11" / "h12" / "h23" / "h24", // optional
    //       }
    //
    //     timeZone: IANA time zone name,
    //
    //     formatOpt: // *second* opt computed in InitializeDateTimeFormat
    //       {
    //         // all the properties/values listed in Table 3
    //         // (weekday, era, year, month, day, &c.)
    //
    //         hour12: true / false,  // optional
    //       }
    //
    //     formatMatcher: "basic" / "best fit",
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every DateTimeFormat lazy data object has *all* these properties,
    // never a subset of them.
    var lazyDateTimeFormatData = std_Object_create(null);

    // Step 1.
    var requestedLocales = CanonicalizeLocaleList(locales);
    lazyDateTimeFormatData.requestedLocales = requestedLocales;

    // Step24.
    options = ToDateTimeOptions(options, "any", "date");

    // Compute options that impact interpretation of locale.
    // Step 3.
    var localeOpt = new Record();
    lazyDateTimeFormatData.localeOpt = localeOpt;

    // Steps 4-5.
    var localeMatcher =
        GetOption(options, "localeMatcher", "string", ["lookup", "best fit"],
                  "best fit");
    localeOpt.localeMatcher = localeMatcher;

    var calendar = GetOption(options, "calendar", "string", undefined, undefined);

    if (calendar !== undefined) {
        calendar = intl_ValidateAndCanonicalizeUnicodeExtensionType(calendar, "calendar", "ca");
    }

    localeOpt.ca = calendar;

    var numberingSystem = GetOption(options, "numberingSystem", "string", undefined, undefined);

    if (numberingSystem !== undefined) {
        numberingSystem = intl_ValidateAndCanonicalizeUnicodeExtensionType(numberingSystem,
                                                                           "numberingSystem",
                                                                           "nu");
    }

    localeOpt.nu = numberingSystem;

    // Step 6.
    var hr12  = GetOption(options, "hour12", "boolean", undefined, undefined);

    // Step 7.
    var hc = GetOption(options, "hourCycle", "string", ["h11", "h12", "h23", "h24"], undefined);

    // Step 8.
    if (hr12 !== undefined) {
        // The "hourCycle" option is ignored if "hr12" is also present.
        hc = null;
    }

    // Step 9.
    localeOpt.hc = hc;

    // Steps 10-16 (see resolveDateTimeFormatInternals).

    // Steps 17-20.
    var tz = options.timeZone;
    if (tz !== undefined) {
        // Step 18.a.
        tz = ToString(tz);

        // Step 18.b.
        var timeZone = intl_IsValidTimeZoneName(tz);
        if (timeZone === null)
            ThrowRangeError(JSMSG_INVALID_TIME_ZONE, tz);

        // Step 18.c.
        tz = CanonicalizeTimeZoneName(timeZone);
    } else {
        // Step 19.
        tz = DefaultTimeZone();
    }
    lazyDateTimeFormatData.timeZone = tz;

    // Step 21.
    var formatOpt = new Record();
    lazyDateTimeFormatData.formatOpt = formatOpt;

    if (mozExtensions) {
        let pattern = GetOption(options, "pattern", "string", undefined, undefined);
        lazyDateTimeFormatData.patternOption = pattern;
    }

    // Step 22.
    // 12.1, Table 5: Components of date and time formats.
    var i, prop;
    for (i = 0; i < dateTimeComponents.length; i++) {
        prop = dateTimeComponents[i];
        var value = GetOption(options, prop, "string", dateTimeComponentValues[prop], undefined);
        formatOpt[prop] = value;
    }

    // Steps 23-24 provided by ICU - see comment after this function.

    // Step 25.
    //
    // For some reason (ICU not exposing enough interface?) we drop the
    // requested format matcher on the floor after this.  In any case, even if
    // doing so is justified, we have to do this work here in case it triggers
    // getters or similar. (bug 852837)
    var formatMatcher =
        GetOption(options, "formatMatcher", "string", ["basic", "best fit"],
                  "best fit");

    // "DateTimeFormat dateStyle & timeStyle" propsal
    // https://github.com/tc39/proposal-intl-datetime-style
    var dateStyle = GetOption(options, "dateStyle", "string", ["full", "long", "medium", "short"],
                              undefined);
    lazyDateTimeFormatData.dateStyle = dateStyle;

    var timeStyle = GetOption(options, "timeStyle", "string", ["full", "long", "medium", "short"],
                              undefined);
    lazyDateTimeFormatData.timeStyle = timeStyle;

    if (dateStyle !== undefined || timeStyle !== undefined) {
      var optionsList = [
          "weekday", "era", "year", "month", "day", "hour", "minute", "second", "timeZoneName",
      ];

      for (var i = 0; i < optionsList.length; i++) {
          var option = optionsList[i];
          if (formatOpt[option] !== undefined) {
              ThrowTypeError(JSMSG_INVALID_DATETIME_OPTION, option,
                             dateStyle !== undefined ? "dateStyle" : "timeStyle");
          }
      }
    }

    // Steps 26-28 provided by ICU, more or less - see comment after this function.

    // Steps 29-30.
    // Pass hr12 on to ICU.
    if (hr12 !== undefined)
        formatOpt.hour12 = hr12;

    // Step 32.
    //
    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(dateTimeFormat, "DateTimeFormat", lazyDateTimeFormatData);

    // 12.2.1, steps 4-5.
    // TODO: spec issue - The current spec doesn't have the IsObject check,
    // which means |Intl.DateTimeFormat.call(null)| is supposed to throw here.
    if (dateTimeFormat !== thisValue && thisValue instanceof GetDateTimeFormatConstructor()) {
        if (!IsObject(thisValue))
            ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, typeof thisValue);

        _DefineDataProperty(thisValue, intlFallbackSymbol(), dateTimeFormat,
                            ATTR_NONENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);

        return thisValue;
    }

    // 12.2.1, step 6.
    return dateTimeFormat;
}


// Intl.DateTimeFormat and ICU skeletons and patterns
// ==================================================
//
// Different locales have different ways to display dates using the same
// basic components. For example, en-US might use "Sept. 24, 2012" while
// fr-FR might use "24 Sept. 2012". The intent of Intl.DateTimeFormat is to
// permit production of a format for the locale that best matches the
// set of date-time components and their desired representation as specified
// by the API client.
//
// ICU supports specification of date and time formats in three ways:
//
// 1) A style is just one of the identifiers FULL, LONG, MEDIUM, or SHORT.
//    The date-time components included in each style and their representation
//    are defined by ICU using CLDR locale data (CLDR is the Unicode
//    Consortium's Common Locale Data Repository).
//
// 2) A skeleton is a string specifying which date-time components to include,
//    and which representations to use for them. For example, "yyyyMMMMdd"
//    specifies a year with at least four digits, a full month name, and a
//    two-digit day. It does not specify in which order the components appear,
//    how they are separated, the localized strings for textual components
//    (such as weekday or month), whether the month is in format or
//    stand-alone form¹, or the numbering system used for numeric components.
//    All that information is filled in by ICU using CLDR locale data.
//    ¹ The format form is the one used in formatted strings that include a
//    day; the stand-alone form is used when not including days, e.g., in
//    calendar headers. The two forms differ at least in some Slavic languages,
//    e.g. Russian: "22 марта 2013 г." vs. "Март 2013".
//
// 3) A pattern is a string specifying which date-time components to include,
//    in which order, with which separators, in which grammatical case. For
//    example, "EEEE, d MMMM y" specifies the full localized weekday name,
//    followed by comma and space, followed by the day, followed by space,
//    followed by the full month name in format form, followed by space,
//    followed by the full year. It
//    still does not specify localized strings for textual components and the
//    numbering system - these are determined by ICU using CLDR locale data or
//    possibly API parameters.
//
// All actual formatting in ICU is done with patterns; styles and skeletons
// have to be mapped to patterns before processing.
//
// The options of DateTimeFormat most closely correspond to ICU skeletons. This
// implementation therefore, in the toBestICUPattern function, converts
// DateTimeFormat options to ICU skeletons, and then lets ICU map skeletons to
// actual ICU patterns. The pattern may not directly correspond to what the
// skeleton requests, as the mapper (UDateTimePatternGenerator) is constrained
// by the available locale data for the locale. The resulting ICU pattern is
// kept as the DateTimeFormat's [[pattern]] internal property and passed to ICU
// in the format method.
//
// An ICU pattern represents the information of the following DateTimeFormat
// internal properties described in the specification, which therefore don't
// exist separately in the implementation:
// - [[weekday]], [[era]], [[year]], [[month]], [[day]], [[hour]], [[minute]],
//   [[second]], [[timeZoneName]]
// - [[hour12]]
// - [[hourCycle]]
// - [[hourNo0]]
// When needed for the resolvedOptions method, the resolveICUPattern function
// maps the instance's ICU pattern back to the specified properties of the
// object returned by resolvedOptions.
//
// ICU date-time skeletons and patterns aren't fully documented in the ICU
// documentation (see http://bugs.icu-project.org/trac/ticket/9627). The best
// documentation at this point is in UTR 35:
// http://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns


/**
 * Returns an ICU pattern string for the given locale and representing the
 * specified options as closely as possible given available locale data.
 */
function toBestICUPattern(locale, options) {
    // Create an ICU skeleton representing the specified options. See
    // http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
    var skeleton = "";
    switch (options.weekday) {
    case "narrow":
        skeleton += "EEEEE";
        break;
    case "short":
        skeleton += "E";
        break;
    case "long":
        skeleton += "EEEE";
    }
    switch (options.era) {
    case "narrow":
        skeleton += "GGGGG";
        break;
    case "short":
        skeleton += "G";
        break;
    case "long":
        skeleton += "GGGG";
        break;
    }
    switch (options.year) {
    case "2-digit":
        skeleton += "yy";
        break;
    case "numeric":
        skeleton += "y";
        break;
    }
    switch (options.month) {
    case "2-digit":
        skeleton += "MM";
        break;
    case "numeric":
        skeleton += "M";
        break;
    case "narrow":
        skeleton += "MMMMM";
        break;
    case "short":
        skeleton += "MMM";
        break;
    case "long":
        skeleton += "MMMM";
        break;
    }
    switch (options.day) {
    case "2-digit":
        skeleton += "dd";
        break;
    case "numeric":
        skeleton += "d";
        break;
    }
    // If hour12 and hourCycle are both present, hour12 takes precedence.
    var hourSkeletonChar = "j";
    if (options.hour12 !== undefined) {
        if (options.hour12)
            hourSkeletonChar = "h";
        else
            hourSkeletonChar = "H";
    } else {
        switch (options.hourCycle) {
        case "h11":
        case "h12":
            hourSkeletonChar = "h";
            break;
        case "h23":
        case "h24":
            hourSkeletonChar = "H";
            break;
        }
    }
    switch (options.hour) {
    case "2-digit":
        skeleton += hourSkeletonChar + hourSkeletonChar;
        break;
    case "numeric":
        skeleton += hourSkeletonChar;
        break;
    }
    switch (options.minute) {
    case "2-digit":
        skeleton += "mm";
        break;
    case "numeric":
        skeleton += "m";
        break;
    }
    switch (options.second) {
    case "2-digit":
        skeleton += "ss";
        break;
    case "numeric":
        skeleton += "s";
        break;
    }
    switch (options.timeZoneName) {
    case "short":
        skeleton += "z";
        break;
    case "long":
        skeleton += "zzzz";
        break;
    }

    // Let ICU convert the ICU skeleton to an ICU pattern for the given locale.
    return intl_patternForSkeleton(locale, skeleton, options.hourCycle);
}


/**
 * Returns a new options object that includes the provided options (if any)
 * and fills in default components if required components are not defined.
 * Required can be "date", "time", or "any".
 * Defaults can be "date", "time", or "all".
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function ToDateTimeOptions(options, required, defaults) {
    assert(typeof required === "string", "ToDateTimeOptions");
    assert(typeof defaults === "string", "ToDateTimeOptions");

    // Steps 1-2.
    if (options === undefined)
        options = null;
    else
        options = ToObject(options);
    options = std_Object_create(options);

    // Step 3.
    var needDefaults = true;

    // Step 4.
    // TODO: spec issue - The spec requires to retrieve all options, so using
    // the ||-operator with its lazy evaluation semantics is incorrect.
    if ((required === "date" || required === "any") &&
        (options.weekday !== undefined || options.year !== undefined ||
         options.month !== undefined || options.day !== undefined))
    {
        needDefaults = false;
    }

    // Step 5.
    // TODO: spec issue - The spec requires to retrieve all options, so using
    // the ||-operator with its lazy evaluation semantics is incorrect.
    if ((required === "time" || required === "any") &&
        (options.hour !== undefined || options.minute !== undefined ||
         options.second !== undefined))
    {
        needDefaults = false;
    }

    // "DateTimeFormat dateStyle & timeStyle" propsal
    // https://github.com/tc39/proposal-intl-datetime-style
    var dateStyle = options.dateStyle;
    var timeStyle = options.timeStyle;

    if (dateStyle !== undefined || timeStyle !== undefined)
        needDefaults = false;

    if (required === "date" && timeStyle !== undefined)
        ThrowTypeError(JSMSG_INVALID_DATETIME_STYLE, "timeStyle", "toLocaleDateString");

    if (required === "time" && dateStyle !== undefined)
        ThrowTypeError(JSMSG_INVALID_DATETIME_STYLE, "dateStyle", "toLocaleTimeString");

    // Step 6.
    if (needDefaults && (defaults === "date" || defaults === "all")) {
        // The specification says to call [[DefineOwnProperty]] with false for
        // the Throw parameter, while Object.defineProperty uses true. For the
        // calls here, the difference doesn't matter because we're adding
        // properties to a new object.
        _DefineDataProperty(options, "year", "numeric");
        _DefineDataProperty(options, "month", "numeric");
        _DefineDataProperty(options, "day", "numeric");
    }

    // Step 7.
    if (needDefaults && (defaults === "time" || defaults === "all")) {
        // See comment for step 7.
        _DefineDataProperty(options, "hour", "numeric");
        _DefineDataProperty(options, "minute", "numeric");
        _DefineDataProperty(options, "second", "numeric");
    }

    // Step 8.
    return options;
}


/**
 * Compares the date and time components requested by options with the available
 * date and time formats in formats, and selects the best match according
 * to a specified basic matching algorithm.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function BasicFormatMatcher(options, formats) {
    // Steps 1-6.
    var removalPenalty = 120,
        additionPenalty = 20,
        longLessPenalty = 8,
        longMorePenalty = 6,
        shortLessPenalty = 6,
        shortMorePenalty = 3;

    // Table 3.
    var properties = ["weekday", "era", "year", "month", "day",
        "hour", "minute", "second", "timeZoneName"];

    // Step 11.c.vi.1.
    var values = ["2-digit", "numeric", "narrow", "short", "long"];

    // Steps 7-8.
    var bestScore = -Infinity;
    var bestFormat;

    // Steps 9-11.
    var i = 0;
    var len = formats.length;
    while (i < len) {
        // Steps 11.a-b.
        var format = formats[i];
        var score = 0;

        // Step 11.c.
        var formatProp;
        for (var j = 0; j < properties.length; j++) {
            var property = properties[j];

            // Step 11.c.i.
            var optionsProp = options[property];
            // Step missing from spec.
            // https://bugs.ecmascript.org/show_bug.cgi?id=1254
            formatProp = undefined;

            // Steps 11.c.ii-iii.
            if (hasOwn(property, format))
                formatProp = format[property];

            if (optionsProp === undefined && formatProp !== undefined) {
                // Step 11.c.iv.
                score -= additionPenalty;
            } else if (optionsProp !== undefined && formatProp === undefined) {
                // Step 11.c.v.
                score -= removalPenalty;
            } else {
                // Step 11.c.vi.
                var optionsPropIndex = callFunction(ArrayIndexOf, values, optionsProp);
                var formatPropIndex = callFunction(ArrayIndexOf, values, formatProp);
                var delta = std_Math_max(std_Math_min(formatPropIndex - optionsPropIndex, 2), -2);
                if (delta === 2)
                    score -= longMorePenalty;
                else if (delta === 1)
                    score -= shortMorePenalty;
                else if (delta === -1)
                    score -= shortLessPenalty;
                else if (delta === -2)
                    score -= longLessPenalty;
            }
        }

        // Step 11.d.
        if (score > bestScore) {
            bestScore = score;
            bestFormat = format;
        }

        // Step 11.e.
        i++;
    }

    // Step 12.
    return bestFormat;
}


/**
 * Compares the date and time components requested by options with the available
 * date and time formats in formats, and selects the best match according
 * to an unspecified best-fit matching algorithm.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function BestFitFormatMatcher(options, formats) {
    // this implementation doesn't have anything better
    return BasicFormatMatcher(options, formats);
}


/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 */
function Intl_DateTimeFormat_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = "DateTimeFormat";

    // Step 2.
    var requestedLocales = CanonicalizeLocaleList(locales);

    // Step 3.
    return SupportedLocales(availableLocales, requestedLocales, options);
}


/**
 * DateTimeFormat internal properties.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.1 and 12.3.3.
 */
var dateTimeFormatInternalProperties = {
    localeData: dateTimeFormatLocaleData,
    relevantExtensionKeys: ["ca", "nu", "hc"]
};


function dateTimeFormatLocaleData() {
    return {
        ca: intl_availableCalendars,
        nu: getNumberingSystems,
        hc: () => {
            return [null, "h11", "h12", "h23", "h24"];
        },
        default: {
            ca: intl_defaultCalendar,
            nu: intl_numberingSystem,
            hc: () => {
                return null;
            }
        }
    };
}


/**
 * Function to be bound and returned by Intl.DateTimeFormat.prototype.format.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.5.
 */
function dateTimeFormatFormatToBind() {
    // Steps 1.a.i-ii
    var date = arguments.length > 0 ? arguments[0] : undefined;
    var x = (date === undefined) ? std_Date_now() : ToNumber(date);

    // Step 1.a.iii.
    return intl_FormatDateTime(this, x, /* formatToParts = */ false);
}

/**
 * Returns a function bound to this DateTimeFormat that returns a String value
 * representing the result of calling ToNumber(date) according to the
 * effective locale and the formatting options of this DateTimeFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.4.3.
 */
function Intl_DateTimeFormat_format_get() {
    // Steps 1-3.
    var dtf = UnwrapDateTimeFormat(this, "format");

    var internals = getDateTimeFormatInternals(dtf);

    // Step 4.
    if (internals.boundFormat === undefined) {
        // Steps 4.a-b.
        var F = callFunction(FunctionBind, dateTimeFormatFormatToBind, dtf);

        // Step 4.c.
        internals.boundFormat = F;
    }

    // Step 5.
    return internals.boundFormat;
}
_SetCanonicalName(Intl_DateTimeFormat_format_get, "get format");


/**
 * Intl.DateTimeFormat.prototype.formatToParts ( date )
 *
 * Spec: ECMAScript Internationalization API Specification, 12.4.4.
 */
function Intl_DateTimeFormat_formatToParts() {
    // Steps 1-3.
    var dtf = UnwrapDateTimeFormat(this, "formatToParts");

    // Ensure the DateTimeFormat internals are resolved.
    getDateTimeFormatInternals(dtf);

    // Steps 4-5.
    var date = arguments.length > 0 ? arguments[0] : undefined;
    var x = (date === undefined) ? std_Date_now() : ToNumber(date);

    // Step 6.
    return intl_FormatDateTime(dtf, x, /* formatToParts = */ true);
}


/**
 * Returns the resolved options for a DateTimeFormat object.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.4.5.
 */
function Intl_DateTimeFormat_resolvedOptions() {
    // Steps 1-3.
    var dtf = UnwrapDateTimeFormat(this, "resolvedOptions");

    var internals = getDateTimeFormatInternals(dtf);

    // Steps 4-5.
    var result = {
        locale: internals.locale,
        calendar: internals.calendar,
        numberingSystem: internals.numberingSystem,
        timeZone: internals.timeZone,
    };

    if (internals.patternOption !== undefined) {
        _DefineDataProperty(result, "pattern", internals.pattern);
    }

    var hasDateStyle = internals.dateStyle !== undefined;
    var hasTimeStyle = internals.timeStyle !== undefined;

    if (hasDateStyle || hasTimeStyle) {
        if (hasTimeStyle) {
            // timeStyle (unlike dateStyle) requires resolving the pattern to
            // ensure "hourCycle" and "hour12" properties are added to |result|.
            resolveICUPattern(internals.pattern, result, /* includeDateTimeFields = */ false);
        }
        if (hasDateStyle) {
            _DefineDataProperty(result, "dateStyle", internals.dateStyle);
        }
        if (hasTimeStyle) {
            _DefineDataProperty(result, "timeStyle", internals.timeStyle);
        }
    } else {
        resolveICUPattern(internals.pattern, result, /* includeDateTimeFields = */ true);
    }

    // Step 6.
    return result;
}


/**
 * Maps an ICU pattern string to a corresponding set of date-time components
 * and their values, and adds properties for these components to the result
 * object, which will be returned by the resolvedOptions method. For the
 * interpretation of ICU pattern characters, see
 * http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
 */
function resolveICUPattern(pattern, result, includeDateTimeFields) {
    assert(IsObject(result), "resolveICUPattern");

    var hourCycle, weekday, era, year, month, day, hour, minute, second,
        timeZoneName;

    var i = 0;
    while (i < pattern.length) {
        var c = pattern[i++];
        if (c === "'") {
            while (i < pattern.length && pattern[i] !== "'")
                i++;
            i++;
        } else {
            var count = 1;
            while (i < pattern.length && pattern[i] === c) {
                i++;
                count++;
            }
            var value;
            switch (c) {
            // "text" cases
            case "G":
            case "E":
            case "z":
            case "v":
            case "V":
                if (count <= 3)
                    value = "short";
                else if (count === 4)
                    value = "long";
                else
                    value = "narrow";
                break;
            // "number" cases
            case "y":
            case "d":
            case "h":
            case "H":
            case "m":
            case "s":
            case "k":
            case "K":
                if (count === 2)
                    value = "2-digit";
                else
                    value = "numeric";
                break;
            // "text & number" cases
            case "M":
            case "L":
                if (count === 1)
                    value = "numeric";
                else if (count === 2)
                    value = "2-digit";
                else if (count === 3)
                    value = "short";
                else if (count === 4)
                    value = "long";
                else
                    value = "narrow";
                break;
            default:
                // skip other pattern characters and literal text
            }

            // Map ICU pattern characters back to the corresponding date-time
            // components of DateTimeFormat. See
            // http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
            switch (c) {
            case "E":
            case "c":
                weekday = value;
                break;
            case "G":
                era = value;
                break;
            case "y":
                year = value;
                break;
            case "M":
            case "L":
                month = value;
                break;
            case "d":
                day = value;
                break;
            case "h":
                hourCycle = "h12";
                hour = value;
                break;
            case "H":
                hourCycle = "h23";
                hour = value;
                break;
            case "k":
                hourCycle = "h24";
                hour = value;
                break;
            case "K":
                hourCycle = "h11";
                hour = value;
                break;
            case "m":
                minute = value;
                break;
            case "s":
                second = value;
                break;
            case "z":
            case "v":
            case "V":
                timeZoneName = value;
                break;
            }
        }
    }
    if (hourCycle) {
        _DefineDataProperty(result, "hourCycle", hourCycle);
        _DefineDataProperty(result, "hour12", hourCycle === "h11" || hourCycle === "h12");
    }
    if (!includeDateTimeFields) {
        return;
    }
    if (weekday) {
        _DefineDataProperty(result, "weekday", weekday);
    }
    if (era) {
        _DefineDataProperty(result, "era", era);
    }
    if (year) {
        _DefineDataProperty(result, "year", year);
    }
    if (month) {
        _DefineDataProperty(result, "month", month);
    }
    if (day) {
        _DefineDataProperty(result, "day", day);
    }
    if (hour) {
        _DefineDataProperty(result, "hour", hour);
    }
    if (minute) {
        _DefineDataProperty(result, "minute", minute);
    }
    if (second) {
        _DefineDataProperty(result, "second", second);
    }
    if (timeZoneName) {
        _DefineDataProperty(result, "timeZoneName", timeZoneName);
    }
}

