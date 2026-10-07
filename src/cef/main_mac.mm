// Adapted from CEF cefsimple: Copyright (c) 2013 The Chromium Embedded
// Framework Authors. Portions copyright (c) 2010 The Chromium Authors.
// CEF-derived portions: BSD license in THIRD_PARTY_NOTICES.md.
// NSApplication integration follows CEF's orderly event-loop shutdown model.
#import <Cocoa/Cocoa.h>

#include <cstdio>

#include "cef/app.h"
#include "cef/client.h"
#include "host/policy.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"

#if !defined(CEF_USE_SANDBOX)
#error "Kunlun Desktop requires CEF_USE_SANDBOX"
#endif

@interface KunlunApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation KunlunApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

- (void)terminate:(id)sender {
  // NSApplication's default exit() would bypass CefShutdown.
  kunlun::CloseDesktopWindow();
}
@end

@interface KunlunApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation KunlunApplicationDelegate
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}
@end

namespace {

void InstallMenu() {
  NSMenu* menu = [[NSMenu alloc] init];
  NSMenuItem* application_item = [[NSMenuItem alloc] init];
  [menu addItem:application_item];
  NSMenu* application_menu = [[NSMenu alloc] initWithTitle:@"Kunlun Desktop"];
  [application_menu addItemWithTitle:@"Quit Kunlun Desktop"
                              action:@selector(terminate:)
                       keyEquivalent:@"q"];
  [application_item setSubmenu:application_menu];

  NSMenuItem* edit_item = [[NSMenuItem alloc] init];
  [menu addItem:edit_item];
  NSMenu* edit_menu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [edit_menu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [edit_menu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
  [edit_item setSubmenu:edit_menu];
  [NSApp setMainMenu:menu];
}

}  // namespace

int main(int argc, char* argv[]) {
  bool smoke_test = false;
  for (int index = 1; index < argc; ++index) {
    if (!kunlun::IsAllowedBrowserArgument(argv[index])) {
      std::fputs("Kunlun Desktop: only --smoke-test is accepted; Chromium "
                 "overrides are forbidden\n",
                 stderr);
      return 2;
    }
    smoke_test = true;
  }

  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    std::fputs("Kunlun Desktop: CEF framework loading failed\n", stderr);
    return 1;
  }
  if (!kunlun::VerifyLoadedCef()) {
    std::fputs("Kunlun Desktop: loaded CEF does not match the compiled pin\n", stderr);
    return 1;
  }

  @autoreleasepool {
    [KunlunApplication sharedApplication];
    if (![NSApp isKindOfClass:[KunlunApplication class]]) {
      std::fputs("Kunlun Desktop: unexpected NSApplication class\n", stderr);
      return 1;
    }
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    KunlunApplicationDelegate* delegate = [[KunlunApplicationDelegate alloc] init];
    [NSApp setDelegate:delegate];
    InstallMenu();

    CefSettings settings;
    settings.no_sandbox = false;
    settings.remote_debugging_port = 0;
    settings.persist_session_cookies = false;
    settings.cookieable_schemes_exclude_defaults = true;
    settings.log_severity = LOGSEVERITY_WARNING;

    // This is a build-tree preview, not an installed product. Keep its profile
    // beside the bundle, and isolate smoke runs from each other and normal use.
    NSString* profile_root = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
    profile_root = [profile_root stringByAppendingPathComponent:@".kunlun-preview"];
    if (smoke_test) {
      profile_root = [profile_root stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
    }
    CefString(&settings.root_cache_path) = [profile_root UTF8String];
    CefString(&settings.cache_path) =
        [[profile_root stringByAppendingPathComponent:@"profile"] UTF8String];

    // No browser_subprocess_path: use CEF's bundled Helper layout. sandbox_info
    // is Windows-only and correctly null on macOS, not a sandbox-disable switch.
    const CefMainArgs main_args(argc, argv);
    const auto app = kunlun::CreateDesktopApp(smoke_test);
    if (!CefInitialize(main_args, settings, app, nullptr)) {
      const int exit_code = CefGetExitCode();
      return exit_code == 0 ? 1 : exit_code;
    }
    std::fputs("Kunlun Desktop: engineering preview; sandbox requested, "
               "unqualified; runtime not connected\n",
               stderr);
    CefRunMessageLoop();
    const int exit_code = kunlun::DesktopExitCode();
    if (smoke_test) {
      std::fputs("Kunlun native smoke: shutting down CEF\n", stderr);
    }
    CefShutdown();
    if (smoke_test) {
      std::fputs("Kunlun native smoke: CEF shutdown complete\n", stderr);
    }
    [NSApp setDelegate:nil];
    return exit_code;
  }
}
