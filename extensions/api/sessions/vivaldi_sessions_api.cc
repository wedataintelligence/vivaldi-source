// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/sessions/vivaldi_sessions_api.h"

#include <algorithm>
#include <string>
#include <vector>
#include "app/vivaldi_constants.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_service_commands.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "ui/vivaldi_session_service.h"

using extensions::vivaldi::sessions_private::SessionItem;

namespace {
enum SessionErrorCodes {
  kNoError,
  kErrorMissingName,
  kErrorWriting,
  kErrorFileMissing,
  kErrorDeleteFailure
};

const base::FilePath::StringType kSessionPath = FILE_PATH_LITERAL("Sessions");
const int kNumberBufferSize = 16;

base::FilePath GenerateFilename(Profile* profile,
                                const std::string session_name,
                                bool unique_name) {
  // PathExists() triggers IO restriction.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string temp_session_name(session_name);
  int cnt = 2;
  char number_string[kNumberBufferSize];

  do {
    base::FilePath path(profile->GetPath());
    path = path.Append(kSessionPath)
#if defined(OS_POSIX)
                 .Append(temp_session_name)
#elif defined(OS_WIN)
                 .Append(base::UTF8ToWide(temp_session_name))
#endif
                 .AddExtension(FILE_PATH_LITERAL(".bin"));
    if (unique_name) {
      if (base::PathExists(path)) {
        temp_session_name.assign(session_name);
        base::snprintf(number_string,
                      kNumberBufferSize,
                      " (%d)", cnt++);
        temp_session_name.append(number_string);
      } else {
        return path;
      }
    } else {
      return path;
    }
    // Just to avoid any endless loop, which is highly unlikely, but still.
  } while (cnt < 1000);

  NOTREACHED();
  return base::FilePath();
}

Browser* GetActiveBrowser() {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    if (browser->window()->IsActive()) {
      return browser;
    }
  }
  return nullptr;
}

}  // namespace

namespace extensions {

SessionsPrivateSaveOpenTabsFunction::SessionsPrivateSaveOpenTabsFunction() {}

SessionsPrivateSaveOpenTabsFunction::~SessionsPrivateSaveOpenTabsFunction() {}

bool SessionsPrivateSaveOpenTabsFunction::RunAsync() {
  scoped_ptr<vivaldi::sessions_private::SaveOpenTabs::Params> params(
      vivaldi::sessions_private::SaveOpenTabs::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int error_code = SessionErrorCodes::kNoError;
  scoped_ptr<::vivaldi::VivaldiSessionService> service(
      new ::vivaldi::VivaldiSessionService(GetProfile()));

  if (params->name.empty()) {
    error_code = SessionErrorCodes::kErrorMissingName;
  } else {
    base::FilePath path = GenerateFilename(GetProfile(), params->name, true);

    for (chrome::BrowserIterator it; !it.done(); it.Next()) {
      Browser* browser = *it;
      // Make sure the browser has tabs and a window. Browser's destructor
      // removes itself from the BrowserList. When a browser is closed the
      // destructor is not necessarily run immediately. This means it's possible
      // for us to get a handle to a browser that is about to be removed. If
      // the tab count is 0 or the window is NULL, the browser is about to be
      // deleted, so we ignore it.
      if (service->ShouldTrackWindow(browser, GetProfile()) &&
          browser->tab_strip_model()->count() && browser->window()) {
        service->BuildCommandsForBrowser(browser);
      }
    }
    bool success = service->Save(path);
    if (!success) {
      error_code = SessionErrorCodes::kErrorWriting;
    }
  }
  results_ =
      vivaldi::sessions_private::SaveOpenTabs::Results::Create(error_code);

  SendResponse(true);
  return true;
}

SessionsPrivateGetAllFunction::SessionsPrivateGetAllFunction() {}

SessionsPrivateGetAllFunction::~SessionsPrivateGetAllFunction() {}

bool SessionsPrivateGetAllFunction::RunAsync() {
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::vector<linked_ptr<SessionItem> > sessions;
  base::FilePath path(GetProfile()->GetPath());
  path = path.Append(kSessionPath);

  base::FileEnumerator iter(path, false, base::FileEnumerator::FILES,
                            FILE_PATH_LITERAL("*.bin"));
  for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
    SessionItem* new_item = new SessionItem;
#if defined(OS_POSIX)
    std::string filename = name.BaseName().value();
#elif defined(OS_WIN)
    std::string filename = base::WideToUTF8(name.BaseName().value());
#endif
    size_t ext = filename.find_last_of('.');
    if (ext != std::string::npos) {
      // Get rid of the extension
      filename.erase(ext, std::string::npos);
    }
    new_item->name.assign(filename);

    linked_ptr<SessionItem> item(new_item);
    sessions.push_back(item);
  }
  results_ =
      vivaldi::sessions_private::GetAll::Results::Create(sessions);

  SendResponse(true);
  return true;
}

SessionsPrivateOpenFunction::SessionsPrivateOpenFunction() {}

SessionsPrivateOpenFunction::~SessionsPrivateOpenFunction() {}

bool SessionsPrivateOpenFunction::RunAsync() {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  scoped_ptr<vivaldi::sessions_private::Open::Params> params(
      vivaldi::sessions_private::Open::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int error_code = SessionErrorCodes::kNoError;
  base::FilePath path = GenerateFilename(GetProfile(), params->name, false);
  if (!base::PathExists(path)) {
    error_code = kErrorFileMissing;
  } else {
    Browser* browser = GetActiveBrowser();
    scoped_ptr<::vivaldi::VivaldiSessionService> service(
        new ::vivaldi::VivaldiSessionService(GetProfile()));
    if (!service->Load(path, browser)) {
      error_code = kErrorFileMissing;
    }
  }
  results_ =
      vivaldi::sessions_private::Open::Results::Create(error_code);

  SendResponse(true);
  return true;
}

SessionsPrivateDeleteFunction::SessionsPrivateDeleteFunction() {}

SessionsPrivateDeleteFunction::~SessionsPrivateDeleteFunction() {}

bool SessionsPrivateDeleteFunction::RunAsync() {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  scoped_ptr<vivaldi::sessions_private::Delete::Params> params(
      vivaldi::sessions_private::Delete::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int error_code = SessionErrorCodes::kNoError;
  base::FilePath path = GenerateFilename(GetProfile(), params->name, false);
  if (!base::PathExists(path)) {
    error_code = kErrorFileMissing;
  } else {
    if (!base::DeleteFile(path, false)) {
      error_code = kErrorDeleteFailure;
    }
  }
  results_ =
      vivaldi::sessions_private::Delete::Results::Create(error_code);

  SendResponse(true);
  return true;
}

}  // namespace extensions
