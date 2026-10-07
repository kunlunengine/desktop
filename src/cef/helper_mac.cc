// Adapted from CEF cefsimple: Copyright (c) 2013 The Chromium Embedded
// Framework Authors. CEF-derived portions: BSD license in THIRD_PARTY_NOTICES.md.
// macOS helper startup follows CEF's sandbox-first, dynamic-loading sequence.
#include <cstdio>

#include "cef/app.h"
#include "include/cef_sandbox_mac.h"
#include "include/wrapper/cef_library_loader.h"

#if !defined(CEF_USE_SANDBOX)
#error "Kunlun Desktop helpers require CEF_USE_SANDBOX"
#endif

int main(int argc, char* argv[]) {
  CefScopedSandboxContext sandbox_context;
  if (!sandbox_context.Initialize(argc, argv)) {
    std::fputs("Kunlun helper: sandbox initialization failed\n", stderr);
    return 1;
  }

  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInHelper()) {
    std::fputs("Kunlun helper: CEF framework loading failed\n", stderr);
    return 1;
  }
  if (!kunlun::VerifyLoadedCef()) {
    std::fputs("Kunlun helper: loaded CEF does not match the compiled pin\n", stderr);
    return 1;
  }

  const CefMainArgs main_args(argc, argv);
  return CefExecuteProcess(main_args, kunlun::CreateDesktopApp(), nullptr);
}
