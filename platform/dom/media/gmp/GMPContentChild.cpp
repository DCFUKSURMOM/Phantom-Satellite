/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentChild.h"
#include "GMPChild.h"
#include "GMPAudioDecoderChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "base/task.h"

namespace mozilla {
namespace gmp {

GMPContentChild::GMPContentChild(GMPChild* aChild)
  : mGMPChild(aChild)
{
  MOZ_COUNT_CTOR(GMPContentChild);
}

GMPContentChild::~GMPContentChild()
{
  MOZ_COUNT_DTOR(GMPContentChild);
}

MessageLoop*
GMPContentChild::GMPMessageLoop()
{
  return mGMPChild->GMPMessageLoop();
}

void
GMPContentChild::CheckThread()
{
  MOZ_ASSERT(mGMPChild->mGMPMessageLoop == MessageLoop::current());
}

void
GMPContentChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mGMPChild->GMPContentChildActorDestroy(this);
}

void
GMPContentChild::ProcessingError(Result aCode, const char* aReason)
{
  mGMPChild->ProcessingError(aCode, aReason);
}

PGMPAudioDecoderChild*
GMPContentChild::AllocPGMPAudioDecoderChild()
{
  return new GMPAudioDecoderChild(this);
}

bool
GMPContentChild::DeallocPGMPAudioDecoderChild(PGMPAudioDecoderChild* aActor)
{
  delete aActor;
  return true;
}

PGMPVideoDecoderChild*
GMPContentChild::AllocPGMPVideoDecoderChild()
{
  GMPVideoDecoderChild* actor = new GMPVideoDecoderChild(this);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor)
{
  static_cast<GMPVideoDecoderChild*>(aActor)->Release();
  return true;
}

PGMPVideoEncoderChild*
GMPContentChild::AllocPGMPVideoEncoderChild()
{
  GMPVideoEncoderChild* actor = new GMPVideoEncoderChild(this);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor)
{
  static_cast<GMPVideoEncoderChild*>(aActor)->Release();
  return true;
}

bool
GMPContentChild::RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor)
{
  auto vdc = static_cast<GMPAudioDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_AUDIO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    return false;
  }

  vdc->Init(static_cast<GMPAudioDecoder*>(vd));

  return true;
}

bool
GMPContentChild::RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor)
{
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    NS_WARNING("GMPGetAPI call failed trying to construct decoder.");
    return false;
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return true;
}

bool
GMPContentChild::RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor)
{
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_ENCODER, &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    NS_WARNING("GMPGetAPI call failed trying to construct encoder.");
    return false;
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return true;
}

void
GMPContentChild::CloseActive()
{
  // Invalidate and remove any remaining API objects.
  const ManagedContainer<PGMPAudioDecoderChild>& audioDecoders =
    ManagedPGMPAudioDecoderChild();
  for (auto iter = audioDecoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PGMPVideoDecoderChild>& videoDecoders =
    ManagedPGMPVideoDecoderChild();
  for (auto iter = videoDecoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PGMPVideoEncoderChild>& videoEncoders =
    ManagedPGMPVideoEncoderChild();
  for (auto iter = videoEncoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }
}

bool
GMPContentChild::IsUsed()
{
  return !ManagedPGMPAudioDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoEncoderChild().IsEmpty();
}

} // namespace gmp
} // namespace mozilla
