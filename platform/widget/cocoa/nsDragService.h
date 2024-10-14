/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h_
#define nsDragService_h_

#include "nsBaseDragService.h"

#include <Cocoa/Cocoa.h>

extern NSString* const kWildcardPboardType;
extern NSString* const kCorePboardType_url;
extern NSString* const kCorePboardType_urld;
extern NSString* const kCorePboardType_urln;
extern NSString* const kCustomTypesPboardType;
#if !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7)
extern NSString* const kCorePboardType_text; // bug 966986 attachment 8540201

// restore from bug 966986
@interface NSPasteboardWrapper : NSObject
{
  NSPasteboard* mPasteboard;
  NSArray* mFilenames;
}
- (id)initWithPasteboard:(NSPasteboard*)aPasteboard;
- (id)propertyListForType:(NSString*)aType;
- (NSPasteboard*)pasteboard;
@end
#endif

class nsDragService : public nsBaseDragService
{
public:
  nsDragService();

  // nsBaseDragService
  virtual nsresult InvokeDragSessionImpl(nsIArray* anArrayTransferables,
                                         nsIScriptableRegion* aRegion,
                                         uint32_t aActionType);
  // nsIDragService
  NS_IMETHOD EndDragSession(bool aDoneDrag);

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable * aTransferable, uint32_t aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, bool *_retval);
  NS_IMETHOD GetNumDropItems(uint32_t * aNumItems);

protected:
  virtual ~nsDragService();

private:

  NSImage* ConstructDragImage(nsIDOMNode* aDOMNode,
                              mozilla::LayoutDeviceIntRect* aDragRect,
                              nsIScriptableRegion* aRegion);
  bool IsValidType(NSString* availableType, bool allowFileURL);
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
  NSString* GetStringForType(NSPasteboardItem* item, const NSString* type,
                             bool allowFileURL = false);
  NSString* GetTitleForURL(NSPasteboardItem* item);
  NSString* GetFilePath(NSPasteboardItem* item);
#endif

  nsCOMPtr<nsIArray> mDataItems; // only valid for a drag started within gecko
  NSView* mNativeDragView;
  NSEvent* mNativeDragEvent;
};

#endif // nsDragService_h_
