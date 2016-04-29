// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/ui/zoom/zoom_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"

namespace content {
class WebContents;
}

namespace favicon {
class FaviconDriver;
}

namespace extensions {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
class TabsEventRouter : public TabStripModelObserver,
                        public BrowserTabStripTrackerDelegate,
                        public chrome::BrowserListObserver,
                        public favicon::FaviconDriverObserver,
                        public ui_zoom::ZoomObserver {
 public:
  explicit TabsEventRouter(Profile* profile);
  ~TabsEventRouter() override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // chrome::BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // TabStripModelObserver:
  void TabInsertedAt(content::WebContents* contents,
                     int index,
                     bool active) override;
  void TabClosingAt(TabStripModel* tab_strip_model,
                    content::WebContents* contents,
                    int index) override;
  void TabDetachedAt(content::WebContents* contents, int index) override;
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabSelectionChanged(TabStripModel* tab_strip_model,
                           const ui::ListSelectionModel& old_model) override;
  void TabMoved(content::WebContents* contents,
                int from_index,
                int to_index) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;
  void TabPinnedStateChanged(content::WebContents* contents,
                             int index) override;

  // ZoomObserver:
  void OnZoomChanged(
      const ui_zoom::ZoomController::ZoomChangedEventData& data) override;

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  // "Synthetic" event. Called from TabInsertedAt if new tab is detected.
  void TabCreatedAt(content::WebContents* contents, int index, bool active);

  // Internal processing of tab updated events. Intended to be called when
  // there's any changed property.
  class TabEntry;
  void TabUpdated(TabEntry* entry,
                  scoped_ptr<base::DictionaryValue> changed_properties);

  // Triggers a tab updated event if the favicon URL changes.
  void FaviconUrlUpdated(content::WebContents* contents);

  // Triggers a tab updated event if the ext data changes.
  void ExtDataUpdated(content::WebContents* contents);

  // The DispatchEvent methods forward events to the |profile|'s event router.
  // The TabsEventRouter listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     scoped_ptr<base::ListValue> args,
                     EventRouter::UserGestureState user_gesture);

  void DispatchEventsAcrossIncognito(
      Profile* profile,
      const std::string& event_name,
      scoped_ptr<base::ListValue> event_args,
      scoped_ptr<base::ListValue> cross_incognito_args);

  // Packages |changed_properties| as a tab updated event for the tab |contents|
  // and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(
      content::WebContents* contents,
      scoped_ptr<base::DictionaryValue> changed_properties);

  // Register ourselves to receive the various notifications we are interested
  // in for a tab. Also create tab entry to observe web contents notifications.
  void RegisterForTabNotifications(content::WebContents* contents);

  // Removes notifications and tab entry added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents* contents);

  // Maintain some information about known tabs, so we can:
  //
  //  - distinguish between tab creation and tab insertion
  //  - not send tab-detached after tab-removed
  //  - reduce the "noise" of TabChangedAt() when sending events to extensions
  //  - remember last muted and audible states to know if there was a change
  //  - listen to WebContentsObserver notifications and forward them to the
  //    event router.
  class TabEntry : public content::WebContentsObserver {
   public:
    // Create a TabEntry associated with, and tracking state changes to,
    // |contents|.
    TabEntry(TabsEventRouter* router, content::WebContents* contents);

    // Indicate via a list of key/value pairs if a tab is loading based on its
    // WebContents. Whether the state has changed or not is used to determine
    // if events needs to be sent to extensions during processing of
    // TabChangedAt(). If this method indicates that a tab should "hold" a
    // state-change to "loading", the DidNavigate() method should eventually
    // send a similar message to undo it. If false, the returned key/value
    // pairs list is empty.
    scoped_ptr<base::DictionaryValue> UpdateLoadState();

    // Indicate via a list of key/value pairs that a tab load has resulted in a
    // navigation and the destination url is available for inspection. The list
    // is empty if no updates should be sent.
    scoped_ptr<base::DictionaryValue> DidNavigate();

    // Indicate via a list of key/value pairs if the title of a tab is changed.
    scoped_ptr<base::DictionaryValue> TitleChanged();

    // Update the audible and muted states and return whether they were changed
    bool SetAudible(bool new_val);
    bool SetMuted(bool new_val);

    // Whether the tab content can be discarded via memory::TabManager.
    bool SetDiscarded(bool new_val);

    // content::WebContentsObserver:
    void ExtDataSet(content::WebContents* contents) override;
    void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) override;
    void TitleWasSet(content::NavigationEntry* entry,
                     bool explicit_set) override;
    void WebContentsDestroyed() override;

   private:
    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the WebContents stops loading after the
    // NAV_ENTRY_COMMITTED was fired. The tab may go back into and out of the
    // loading state subsequently, but we will ignore those changes.
    bool complete_waiting_on_load_;

    // Previous audible and muted states
    bool was_audible_;
    bool was_muted_;

    // Previous discarded state, see memory::TabManager.
    bool was_discarded_;

    GURL url_;

    base::string16 title_;

    // Event router that the WebContents's noficiations are forwarded to.
    TabsEventRouter* router_;

    DISALLOW_COPY_AND_ASSIGN(TabEntry);
  };

  // Gets the TabEntry for the given |contents|. Returns TabEntry* if found,
  // nullptr if not.
  TabEntry* GetTabEntry(content::WebContents* contents);

  using TabEntryMap = std::map<int, scoped_ptr<TabEntry>>;
  TabEntryMap tab_entries_;

  // The main profile that owns this event router.
  Profile* profile_;

  ScopedObserver<favicon::FaviconDriver, TabsEventRouter>
      favicon_scoped_observer_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabsEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_