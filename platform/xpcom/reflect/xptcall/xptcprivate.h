/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* All the xptcall private declarations - only include locally. */

#ifndef xptcprivate_h___
#define xptcprivate_h___

#include "xptcall.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class xptiInterfaceEntry;

#define STUB_ENTRY(n) NS_IMETHOD Stub##n() = 0;

#define SENTINEL_ENTRY(n) NS_IMETHOD Sentinel##n() = 0;

class nsIXPTCStubBase : public nsISupports
{
public:
#include "xptcstubsdef.inc"
};

#undef STUB_ENTRY
#undef SENTINEL_ENTRY

#define STUB_ENTRY(n) NS_IMETHOD Stub##n() override;

#define SENTINEL_ENTRY(n) NS_IMETHOD Sentinel##n() override;

class nsXPTCStubBase final : public nsIXPTCStubBase
{
public:
    NS_DECL_ISUPPORTS_INHERITED

#include "xptcstubsdef.inc"

    nsXPTCStubBase(nsIXPTCProxy* aOuter, xptiInterfaceEntry *aEntry)
        : mOuter(aOuter), mEntry(aEntry) { MOZ_COUNT_CTOR(nsXPTCStubBase); }

    nsIXPTCProxy*          mOuter;
    xptiInterfaceEntry*    mEntry;

    ~nsXPTCStubBase() { MOZ_COUNT_DTOR(nsXPTCStubBase); }
};

#undef STUB_ENTRY
#undef SENTINEL_ENTRY

#if defined(__clang__) || defined(__GNUC__)
#define ATTRIBUTE_USED __attribute__ ((__used__))
#else
#define ATTRIBUTE_USED
#endif

#endif /* xptcprivate_h___ */
