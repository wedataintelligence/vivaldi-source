// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_switches.h"
#include "notes/notesnode.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_model_loaded_observer.h"
#include "prefs/vivaldi_pref_names.h"

#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/notes/notes_api.h"
#include "extensions/api/import_data/import_data_api.h"
#include "extensions/api/show_menu/show_menu_api.h"
#include "extensions/api/settings/settings_api.h"
#include "extensions/api/zoom/zoom_api.h"
#include "extensions/vivaldi_extensions_init.h"

VivaldiBrowserMainExtraParts::VivaldiBrowserMainExtraParts() {
}

VivaldiBrowserMainExtraParts::~VivaldiBrowserMainExtraParts() {
}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraParts::PostEarlyInitialization() {
  base::CommandLine *command_line = base::CommandLine::ForCurrentProcess();
  if (vivaldi::IsVivaldiRunning()) {
    // andre@vivaldi.com HACK while not having all the permission dialogs in
    // place.
    command_line->AppendSwitchNoDup(
        switches::kAlwaysAuthorizePlugins);

    command_line->AppendSwitchNoDup(
      translate::switches::kDisableTranslate);
  }

#if defined(OS_MACOSX)
  PostEarlyInitializationMac();
#endif
#if defined(OS_WIN)
  PostEarlyInitializationWin();
#endif
#if defined(OS_LINUX)
  PostEarlyInitializationLinux();
#endif
}

void VivaldiBrowserMainExtraParts::
     EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::ExtensionActionUtilFactory::GetInstance();
  extensions::ImportDataAPI::GetFactoryInstance();
  extensions::NotesAPI::GetFactoryInstance();
  extensions::ShowMenuAPI::GetFactoryInstance();
  extensions::VivaldiExtensionInit::GetFactoryInstance();
  extensions::VivaldiSettingsApiNotificationFactory::GetInstance();
  extensions::ZoomAPI::GetFactoryInstance();
}

void VivaldiBrowserMainExtraParts::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

void VivaldiBrowserMainExtraParts::PostProfileInit() {
  if (!vivaldi::IsVivaldiRunning())
    return;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->GetPrefs()->GetBoolean(vivaldiprefs::kSmoothScrollingEnabled) ==
      false) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup(
        switches::kDisableSmoothScrolling);
  }

  vivaldi::Notes_Model* notes_model =
      vivaldi::NotesModelFactory::GetForProfile(profile);
  notes_model->AddObserver(new vivaldi::NotesModelLoadedObserver(profile));
}
