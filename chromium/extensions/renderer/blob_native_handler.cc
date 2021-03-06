// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/blob_native_handler.h"

#include "base/bind.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebBlob.h"

namespace {

// Expects a single Blob argument. Returns the Blob's UUID.
void GetBlobUuid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  blink::WebBlob blob = blink::WebBlob::fromV8Value(args[0]);
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(), blob.uuid().utf8().data()));
}

}  // namespace

namespace extensions {

BlobNativeHandler::BlobNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetBlobUuid", base::Bind(&GetBlobUuid));
  RouteFunction("TakeBrowserProcessBlob",
                base::Bind(&BlobNativeHandler::TakeBrowserProcessBlob,
                           base::Unretained(this)));
}

// Take ownership of a Blob created on the browser process. Expects the Blob's
// UUID, type, and size as arguments. Returns the Blob we just took to
// Javascript. The Blob reference in the browser process is dropped through
// a separate flow to avoid leaking Blobs if the script context is destroyed.
void BlobNativeHandler::TakeBrowserProcessBlob(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsInt32());
  std::string uuid(*v8::String::Utf8Value(args[0]));
  std::string type(*v8::String::Utf8Value(args[1]));
  blink::WebBlob blob =
      blink::WebBlob::createFromUUID(blink::WebString::fromUTF8(uuid),
                                     blink::WebString::fromUTF8(type),
                                     args[2]->Int32Value());
  args.GetReturnValue().Set(blob.toV8Value(
      context()->v8_context()->Global(), args.GetIsolate()));
}

}  // namespace extensions
