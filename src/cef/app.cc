// Views lifecycle adapted from CEF cefsimple: Copyright (c) 2013 The Chromium
// Embedded Framework Authors. BSD license in THIRD_PARTY_NOTICES.md.
#include "cef/app.h"

#include "cef/client.h"
#include "host/policy.h"
#include "include/cef_browser.h"
#include "include/cef_scheme.h"
#include "include/cef_version_info.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace kunlun {
namespace {

class WindowDelegate final : public CefWindowDelegate {
 public:
  explicit WindowDelegate(CefRefPtr<CefBrowserView> view) : view_(view) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->SetTitle("Kunlun Desktop — Engineering preview");
    window->AddChildView(view_);
    window->Show();
    view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override { view_ = nullptr; }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    const auto browser = view_ ? view_->GetBrowser() : nullptr;
    return !browser || browser->GetHost()->TryCloseBrowser();
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override { return CefSize(1100, 820); }

  CefSize GetMinimumSize(CefRefPtr<CefView> view) override { return CefSize(640, 540); }

  cef_runtime_style_t GetWindowRuntimeStyle() override { return CEF_RUNTIME_STYLE_ALLOY; }

 private:
  CefRefPtr<CefBrowserView> view_;
  IMPLEMENT_REFCOUNTING(WindowDelegate);
};

class BrowserViewDelegate final : public CefBrowserViewDelegate {
 public:
  cef_runtime_style_t GetBrowserRuntimeStyle() override { return CEF_RUNTIME_STYLE_ALLOY; }

 private:
  IMPLEMENT_REFCOUNTING(BrowserViewDelegate);
};

class DesktopApp final : public CefApp,
                         public CefBrowserProcessHandler,
                         public CefRenderProcessHandler {
 public:
  explicit DesktopApp(bool smoke_test) : smoke_test_(smoke_test) {}

  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
    // Register identically in browser and helper processes. Do not mark this as
    // local/file-like, fetch-enabled, or CSP-bypassing.
    registrar->AddCustomScheme("kunlun", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                                             CEF_SCHEME_OPTION_CORS_ENABLED);
  }

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

  void OnContextInitialized() override {
    CEF_REQUIRE_UI_THREAD();
    CefBrowserSettings settings;
    settings.background_color = CefColorSetARGB(255, 16, 20, 26);
    settings.javascript_close_windows = STATE_DISABLED;
    const auto view = CefBrowserView::CreateBrowserView(
        CreateDesktopClient(smoke_test_), std::string(kDocumentUrl), settings, nullptr, nullptr,
        new BrowserViewDelegate());
    CefWindow::CreateTopLevelWindow(new WindowDelegate(view));
  }

  void OnWebKitInitialized() override {
    CEF_REQUIRE_RENDERER_THREAD();
    renderer_router_ = CefMessageRouterRendererSide::Create(HostRouterConfig());
  }

  void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();
    if (frame->IsMain() && IsTrustedDocument(frame->GetURL().ToString())) {
      renderer_router_->OnContextCreated(browser, frame, context);
    }
  }

  void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();
    renderer_router_->OnContextReleased(browser, frame, context);
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    CEF_REQUIRE_RENDERER_THREAD();
    return source_process == PID_BROWSER &&
           renderer_router_->OnProcessMessageReceived(browser, frame, source_process, message);
  }

 private:
  const bool smoke_test_;
  CefRefPtr<CefMessageRouterRendererSide> renderer_router_;
  IMPLEMENT_REFCOUNTING(DesktopApp);
};

}  // namespace

CefMessageRouterConfig HostRouterConfig() {
  CefMessageRouterConfig config;
  config.js_query_function = "kunlunQuery";
  config.js_cancel_function = "kunlunQueryCancel";
  return config;
}

bool VerifyLoadedCef() {
  const char* version = cef_version_full();
  return version && std::string_view(version) == CEF_VERSION;
}

CefRefPtr<CefApp> CreateDesktopApp(bool smoke_test) { return new DesktopApp(smoke_test); }

}  // namespace kunlun
