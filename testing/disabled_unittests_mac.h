// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(BrowserTest, BeforeUnloadVsBeforeReload)

  DISABLE(DevToolsSanityTest, TestNetworkSize)
  DISABLE(DevToolsSanityTest, TestNetworkRawHeadersText)

  DISABLE(DumpAccessibilityTreeTest, AccessibilityInputDate)
  DISABLE(DumpAccessibilityTreeTest, AccessibilityInputTime)

  DISABLE(SearchProviderTest, TestIsSearchProviderInstalled)

  DISABLE(SitePerProcessBrowserTest, FrameOwnerPropertiesPropagationScrolling)

  DISABLE(TabCaptureApiTest, GrantForChromePages)

  DISABLE(WebViewInteractiveTest, NewWindow_WebViewNameTakesPrecedence)
  DISABLE(WebViewInteractiveTest, NewWindow_Redirect)

  // TODO: Disabled in Vivaldi due to WebKit keyhandling changes
  DISABLE(WebViewInteractiveTest, PopupPositioningBasic)
  DISABLE(WebViewInteractiveTest, PopupPositioningMoved)
  // *******

  // Suspected regressions since v47 (some pre-v47 disablings included, though)
  DISABLE(AppShimInteractiveTest, Launch)
  DISABLE(AppShimInteractiveTest, ShowWindow)

  DISABLE(BrowserActionButtonUiTest, AddExtensionWithMenuOpen)
  DISABLE(BrowserActionButtonUiTest, ContextMenusOnMainAndOverflow)
  DISABLE(BrowserActionButtonUiTest, TestOverflowContainerLayout)

  DISABLE(BrowserWindowCocoaTest, TestMinimizeState)

  DISABLE(BrowserWindowFullScreenControllerTest, TestActivate)
  DISABLE(BrowserWindowFullScreenControllerTest, TestFullscreen)

  DISABLE(ConstrainedWindowMacTest, BrowserWindowFullscreen)
  DISABLE(DevToolsSanityTest, TestNetworkSyncSize)
  DISABLE(DumpAccessibilityTreeTest, AccessibilityContenteditableDescendants)
  DISABLE(DumpAccessibilityTreeTest, AccessibilityContenteditableDescendantsWithSelection)

  DISABLE(ExtensionApiTest, Bookmarks)
  DISABLE(ExtensionApiTest, FocusWindowDoesNotExitFullscreen)

  DISABLE(ExtensionInstallUIBrowserTest, TestInstallThemeInFullScreen)

  DISABLE(FullscreenControllerTest, PermissionContentSettings)

  DISABLE(MediaScanManagerTest, MergeRedundantVerifyNoOvercount)
  DISABLE(MediaScanManagerTest, SingleResult)

  DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, Minimize)
  DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, MinimizeMaximize)

  DISABLE(PlatformAppBrowserTest, WindowsApiProperties)

  DISABLE(PolicyTest, Disable3DAPIs)

  DISABLE(PrerenderBrowserTest, PrerenderExcessiveMemory)

  DISABLE(ProfileListDesktopBrowserTest, SignOut)

  DISABLE(ScreenCaptureNotificationUICocoaTest, MinimizeWindow)
  DISABLE(ServiceProcessControlMac, TestGTMSMJobSubmitRemove)

  DISABLE(ShowAppListNonDefaultInteractiveTest, ShowAppListNonDefaultProfile)

  DISABLE(SpinnerViewTest, StopAnimationOnMiniaturize)
  DISABLE(SpriteViewTest, StopAnimationOnMiniaturize)

  DISABLE(StackedPanelBrowserTest, AddNewPanelNotWithSystemMinimizedDetachedPanel)
  DISABLE(StackedPanelBrowserTest, AddNewPanelNotWithSystemMinimizedStack)
