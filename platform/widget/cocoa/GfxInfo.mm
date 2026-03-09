/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "mozilla/ArrayUtils.h"

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "nsCocoaFeatures.h"
#include "mozilla/Preferences.h"
#include <algorithm>

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <Cocoa/Cocoa.h>

using namespace mozilla;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

GfxInfo::GfxInfo()
{
}

static OperatingSystem
OSXVersionToOperatingSystem(uint32_t aOSXVersion) {
  switch (nsCocoaFeatures::ExtractMajorVersion(aOSXVersion)) {
    case 10:
      switch (nsCocoaFeatures::ExtractMinorVersion(aOSXVersion)) {
        case 5:
          return OperatingSystem::OSX10_5;
        case 6:
          return OperatingSystem::OSX10_6;
        case 7:
          return OperatingSystem::OSX10_7;
        case 8:
          return OperatingSystem::OSX10_8;
        case 9:
          return OperatingSystem::OSX10_9;
        case 10:
          return OperatingSystem::OSX10_10;
        case 11:
          return OperatingSystem::OSX10_11;
        case 12:
          return OperatingSystem::OSX10_12;
        case 13:
          return OperatingSystem::OSX10_13;
        case 14:
          return OperatingSystem::OSX10_14;
        case 15:
          return OperatingSystem::OSX10_15;
        case 16:
          // Depending on the SDK version, we either get 10.16 or 11.0.
          // Normalize this to 11.0.
          return OperatingSystem::OSX11_0;
        default:
          break;
      }
      break;
    case 11:
      return OperatingSystem::OSX11_0;
    case 12:
      return OperatingSystem::OSX12_0;
    case 13:
      return OperatingSystem::OSX13_0;
    case 14:
      return OperatingSystem::OSX14_0;
    default:
      break;
  }

  return OperatingSystem::Unknown;
}
// The following three functions are derived from Chromium code
static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                       CFStringRef propertyName)
{
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}

static uint32_t IntValueOfCFData(CFDataRef d)
{
  uint32_t value = 0;

  if (d) {
    const uint32_t *vp = reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(d));
    if (vp != NULL)
      value = *vp;
  }

  return value;
}

void
GfxInfo::GetDeviceInfo()
{
  io_registry_entry_t dsp_port = CGDisplayIOServicePort(kCGDirectMainDisplay);
  CFTypeRef vendor_id_ref = SearchPortForProperty(dsp_port, CFSTR("vendor-id"));
  if (vendor_id_ref) {
    mAdapterVendorID.AppendPrintf("0x%04x", IntValueOfCFData((CFDataRef)vendor_id_ref));
    CFRelease(vendor_id_ref);
  }
  CFTypeRef device_id_ref = SearchPortForProperty(dsp_port, CFSTR("device-id"));
  if (device_id_ref) {
    mAdapterDeviceID.AppendPrintf("0x%04x", IntValueOfCFData((CFDataRef)device_id_ref));
    CFRelease(device_id_ref);
  }
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
  CFMutableDictionaryRef matchingDict = IOServiceMatching("IOAccelerator");
  io_iterator_t iterator;
  if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator) == kIOReturnSuccess) {
    io_registry_entry_t registryEntry;
    while ((registryEntry = IOIteratorNext(iterator))) {
      CFTypeRef bundleName = IORegistryEntryCreateCFProperty(registryEntry,
                                                             CFSTR("IOGLBundleName"),
                                                             kCFAllocatorDefault,
                                                             0);
      if (bundleName) {
        if (CFGetTypeID(bundleName) == CFStringGetTypeID()) {
          NSString* str = (NSString*)bundleName;
          mIOGLBundleName.Assign(NS_ConvertUTF8toUTF16([str UTF8String]));
          fprintf(stderr, "GfxInfo::GetDeviceInfo: IOGLBundleName: %s\n", NS_ConvertUTF16toUTF8(mIOGLBundleName).get());
        }
        CFRelease(bundleName);
      }
      IOObjectRelease(registryEntry);
      if (!mIOGLBundleName.IsEmpty()) break;
    }
    IOObjectRelease(iterator);
  }

  if (mIOGLBundleName.IsEmpty()) {
    fprintf(stderr, "GfxInfo::GetDeviceInfo: No IOGLBundleName found\n");
  }
#endif
}

nsresult
GfxInfo::Init()
{
  nsresult rv = GfxInfoBase::Init();

  // Calling CGLQueryRendererInfo causes us to switch to the discrete GPU
  // even when we don't want to. We'll avoid doing so for now and just
  // use the device ids.

  GetDeviceInfo();

  AddCrashReportAnnotations();

  mOSXVersion = nsCocoaFeatures::macOSVersion();

  return rv;
}

NS_IMETHODIMP
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString cleartypeParameters; */
NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString & aCleartypeParams)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM = mAdapterRAMString;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID;
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID;
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterSubsysID; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString & aAdapterSubsysID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterSubsysID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString & aAdapterSubsysID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  return NS_ERROR_FAILURE;
}

void
GfxInfo::AddCrashReportAnnotations()
{
  /*** STUB ***/
}

// We don't support checking driver versions on Mac.
#define IMPLEMENT_MAC_DRIVER_BLOCKLIST(os, vendor, device, features, blockOn, ruleId) \
  APPEND_TO_DRIVER_BLOCKLIST(os, vendor, device, features, blockOn,           \
                             DRIVER_COMPARISON_IGNORED, V(0,0,0,0), ruleId, "")


const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  if (!mDriverInfo->Length()) {
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(OperatingSystem::OSX,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_MSAA, nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION, "FEATURE_FAILURE_MAC_ATI_NO_MSAA");
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(OperatingSystem::OSX,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(Geforce7300GT),
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_BLOCKED_DEVICE, "FEATURE_FAILURE_MAC_7300_NO_WEBGL");
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(OperatingSystem::OSX,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(GeforceFX),
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE, "FEATURE_FAILURE_MAC_GEFORCEFX_NO_LAYERS");
  }
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(int32_t aFeature,
                              int32_t* aStatus,
                              nsAString& aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo,
                              nsACString& aFailureId,
                              OperatingSystem* aOS /* = nullptr */)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  OperatingSystem os = OSXVersionToOperatingSystem(mOSXVersion);
  if (aOS)
    *aOS = os;

  if (mShutdownOccurred) {
    return NS_OK;
  }

  // Don't evaluate special cases when we're evaluating the downloaded blocklist.
  if (!aDriverInfo.Length()) {
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
    // Block the non-QECI drivers for OpenGL. They just crash the browser
    // We only need this for 10.5 (PPC) as all drivers on Intel work.
    if (mIOGLBundleName.EqualsLiteral("ATIRadeon8500GLDriver") ||
        mIOGLBundleName.EqualsLiteral("GeForce2MXGLDriver") ||
        mIOGLBundleName.EqualsLiteral("GeForce3GLDriver")) {
      if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS) {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        aFailureId = "FEATURE_FAILURE_MAC_UNSUPPORTED_GPU";
        return NS_OK;
      }
    }

    // Many WebGL issues on 10.5, especially:
    //   * bug 631258: WebGL shader paints using textures belonging to other processes on Mac OS 10.5
    //   * bug 618848: Post process shaders and texture mapping crash OS X 10.5
    if (aFeature == nsIGfxInfo::FEATURE_WEBGL_OPENGL &&
        !nsCocoaFeatures::OnSnowLeopardOrLater()) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
      aFailureId = "FEATURE_FAILURE_WEBGL_OSX_VERSION";
      return NS_OK;
    }
#endif
    if (aFeature == nsIGfxInfo::FEATURE_WEBGL_MSAA) {
      // Blacklist all ATI cards on OSX, except for
      // 0x6760 and 0x9488
      if (mAdapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorATI), nsCaseInsensitiveStringComparator()) &&
          (mAdapterDeviceID.LowerCaseEqualsLiteral("0x6760") ||
           mAdapterDeviceID.LowerCaseEqualsLiteral("0x9488"))) {
        *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
        return NS_OK;
      }
    } else if (aFeature == nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION) {
      // See bug 1249659
      switch(os) {
        case OperatingSystem::OSX10_5:
        case OperatingSystem::OSX10_6:
        case OperatingSystem::OSX10_7:
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
          aFailureId = "FEATURE_FAILURE_CANVAS_OSX_VERSION";
          break;
        default:
          *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
          break;
      }
      return NS_OK;
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, aFailureId, &os);
}

nsresult
GfxInfo::FindMonitors(JSContext* aCx, JS::HandleObject aOutArray)
{
  // Getting the refresh rate is a little hard on OS X. We could use
  // CVDisplayLinkGetNominalOutputVideoRefreshPeriod, but that's a little
  // involved. Ideally we could query it from vsync. For now, we leave it out.
  int32_t deviceCount = 0;
  for (NSUInteger i = 0; i < [[NSScreen screens] count]; i++) {
	NSScreen *screen = (NSScreen *)[[NSScreen screens] objectAtIndex:i];
    NSRect rect = [screen frame];

    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

    JS::Rooted<JS::Value> screenWidth(aCx, JS::Int32Value((int)rect.size.width));
    JS_SetProperty(aCx, obj, "screenWidth", screenWidth);

    JS::Rooted<JS::Value> screenHeight(aCx, JS::Int32Value((int)rect.size.height));
    JS_SetProperty(aCx, obj, "screenHeight", screenHeight);

    JS::Rooted<JS::Value> scale(aCx, JS::NumberValue(nsCocoaUtils::GetBackingScaleFactor(screen)));
    JS_SetProperty(aCx, obj, "scale", scale);

    JS::Rooted<JS::Value> element(aCx, JS::ObjectValue(*obj));
    JS_SetElement(aCx, aOutArray, deviceCount++, element);
  }
  return NS_OK;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  mAdapterVendorID = aVendorID;
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  mAdapterDeviceID = aDeviceID;
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  mDriverVersion = aDriverVersion;
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  mOSXVersion = aVersion;
  return NS_OK;
}

#endif
