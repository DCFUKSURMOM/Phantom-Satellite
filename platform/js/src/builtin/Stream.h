/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "builtin/Promise.h"
#include "vm/NativeObject.h"


namespace js {

class AutoSetNewObjectMetadata;

class ReadableStream : public NativeObject
{
  public:
    static ReadableStream* createDefaultStream(JSContext* cx, HandleValue underlyingSource,
                                               HandleValue size, HandleValue highWaterMark,
                                               HandleObject proto = nullptr);
    static ReadableStream* createByteStream(JSContext* cx, HandleValue underlyingSource,
                                            HandleValue highWaterMark,
                                            HandleObject proto = nullptr);
    static ReadableStream* createExternalSourceStream(JSContext* cx, void* underlyingSource,
                                                      uint8_t flags, HandleObject proto = nullptr);

    bool readable() const;
    bool closed() const;
    bool errored() const;
    bool disturbed() const;

    bool locked() const;

    void desiredSize(bool* hasSize, double* size) const;

    JS::ReadableStreamMode mode() const;

    [[nodiscard]] static bool close(JSContext* cx, Handle<ReadableStream*> stream);
    [[nodiscard]] static JSObject* cancel(JSContext* cx, Handle<ReadableStream*> stream,
                                          HandleValue reason);
    [[nodiscard]] static bool error(JSContext* cx, Handle<ReadableStream*> stream,
                                    HandleValue error);

    [[nodiscard]] static NativeObject* getReader(JSContext* cx, Handle<ReadableStream*> stream,
                                                 JS::ReadableStreamReaderMode mode);

    [[nodiscard]] static bool tee(JSContext* cx,
                                  Handle<ReadableStream*> stream, bool cloneForBranch2,
                                  MutableHandle<ReadableStream*> branch1Stream,
                                  MutableHandle<ReadableStream*> branch2Stream);

    [[nodiscard]] static bool enqueue(JSContext* cx, Handle<ReadableStream*> stream,
                                      HandleValue chunk);
    [[nodiscard]] static bool enqueueBuffer(JSContext* cx, Handle<ReadableStream*> stream,
                                            Handle<ArrayBufferObject*> chunk);
    [[nodiscard]] static bool getExternalSource(JSContext* cx, Handle<ReadableStream*> stream,
                                                void** source);
    void releaseExternalSource();
    uint8_t embeddingFlags() const;
    [[nodiscard]] static bool updateDataAvailableFromSource(JSContext* cx,
                                                            Handle<ReadableStream*> stream,
                                                            uint32_t availableData);

    enum State {
         Readable  = 1 << 0,
         Closed    = 1 << 1,
         Errored   = 1 << 2,
         Disturbed = 1 << 3
    };

  private:
    [[nodiscard]] static ReadableStream* createStream(JSContext* cx, HandleObject proto = nullptr);

  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamDefaultReader : public NativeObject
{
  public:
    [[nodiscard]] static JSObject* read(JSContext* cx, Handle<ReadableStreamDefaultReader*> reader);

    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamBYOBReader : public NativeObject
{
  public:
    [[nodiscard]] static JSObject* read(JSContext* cx, Handle<ReadableStreamBYOBReader*> reader,
                                        Handle<ArrayBufferViewObject*> view);

    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

bool ReadableStreamReaderIsClosed(const JSObject* reader);

[[nodiscard]] bool ReadableStreamReaderCancel(JSContext* cx, HandleObject reader,
                                             HandleValue reason);

[[nodiscard]] bool ReadableStreamReaderReleaseLock(JSContext* cx, HandleObject reader);

class ReadableStreamDefaultController : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableByteStreamController : public NativeObject
{
  public:
    bool hasExternalSource();

    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamBYOBRequest : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ByteLengthQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class CountQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

} // namespace js

#endif /* builtin_Stream_h */
