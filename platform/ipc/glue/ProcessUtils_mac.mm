/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "nsString.h"
#ifdef MOZ_ENABLE_NPAPI
#include "mozilla/plugins/PluginUtilsOSX.h"
#else
#include <pthread.h>
#endif

namespace mozilla {
namespace ipc {

void SetThisProcessName(const char *aName)
{
#ifdef MOZ_ENABLE_NPAPI
  mozilla::plugins::PluginUtilsOSX::SetProcessName(aName);
#else
  pthread_setname_np(aName);
#endif
}

} // namespace ipc
} // namespace mozilla
