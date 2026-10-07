#include "cef/client.h"

#include <climits>
#include <cstdio>
#include <string>

#include "cef/app.h"
#include "cef/bundle.h"
#include "cef/smoke_test.h"
#include "host/policy.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/cef_version.h"
#include "include/wrapper/cef_helpers.h"

namespace kunlun {
namespace {

class DesktopClient;
DesktopClient* g_client = nullptr;  // Non-owning, browser UI thread only.
int g_exit_code = 0;

class LockedResourceHandler final : public CefResourceRequestHandler {
 public:
  ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   CefRefPtr<CefCallback> callback) override {
    CEF_REQUIRE_IO_THREAD();
    const auto url = request->GetURL().ToString();
    if (!browser || !frame || !frame->IsMain() || request->GetMethod() != "GET" ||
        !FindBundledAsset(url)) {
      return RV_CANCEL;
    }
    // The client's request hook already checked CEF's navigation/initiator
    // identity. The frame's committed URL can lag initial subresource requests.
    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefFrame> frame,
                                                   CefRefPtr<CefRequest> request) override {
    CEF_REQUIRE_IO_THREAD();
    return CreateBundleResource(request->GetURL().ToString());
  }

  void OnProtocolExecution(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefRequest> request, bool& allow_os_execution) override {
    allow_os_execution = false;
  }

 private:
  IMPLEMENT_REFCOUNTING(LockedResourceHandler);
};

class SmokeDeadline final : public CefTask {
 public:
  void Execute() override;

 private:
  IMPLEMENT_REFCOUNTING(SmokeDeadline);
};

class DesktopClient final : public CefClient,
                            public CefLifeSpanHandler,
                            public CefRequestHandler,
                            public CefLoadHandler,
                            public CefDisplayHandler,
                            public CefContextMenuHandler,
                            public CefDownloadHandler,
                            public CefPermissionHandler,
                            public CefMessageRouterBrowserSide::Handler {
 public:
  explicit DesktopClient(bool smoke_test)
      : smoke_test_(smoke_test),
        resources_(new LockedResourceHandler()),
        router_(CefMessageRouterBrowserSide::Create(HostRouterConfig())) {
    CEF_REQUIRE_UI_THREAD();
    g_client = this;
    g_exit_code = smoke_test ? 1 : 0;
    router_->AddHandler(this, false);
  }

  ~DesktopClient() override {
    if (g_client == this) {
      g_client = nullptr;
    }
  }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
    if (smoke_test_) {
      CefPostDelayedTask(TID_UI, new SmokeDeadline(), 15000);
    }
    if (closing_) {
      browser_->GetHost()->CloseBrowser(true);
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    router_->OnBeforeClose(browser);
    router_->RemoveHandler(this);
    browser_ = nullptr;
    if (smoke_test_) {
      std::fputs("Kunlun native smoke: browser closed\n", stderr);
    }
    CefQuitMessageLoop();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id,
                     const CefString& target_url, const CefString& target_frame_name,
                     WindowOpenDisposition target_disposition, bool user_gesture,
                     const CefPopupFeatures& popup_features, CefWindowInfo& window_info,
                     CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
    return true;
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override {
    CEF_REQUIRE_UI_THREAD();
    if (!frame->IsMain() || request->GetMethod() != "GET" ||
        !IsTrustedDocument(request->GetURL().ToString())) {
      return true;
    }
    router_->OnBeforeBrowse(browser, frame);
    return false;
  }

  bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        const CefString& target_url, WindowOpenDisposition disposition,
                        bool user_gesture) override {
    return true;
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request,
      bool is_navigation, bool is_download, const CefString& request_initiator,
      bool& disable_default_handling) override {
    // No fallback to the network/file/OS protocol loaders, even on a miss.
    disable_default_handling = true;
    if (!browser || !frame || !frame->IsMain() || is_download || request->GetMethod() != "GET") {
      return nullptr;
    }
    if (is_navigation) {
      return IsTrustedDocument(request->GetURL().ToString()) ? resources_ : nullptr;
    }
    const auto origin = kOrigin.substr(0, kOrigin.size() - 1);
    if (request_initiator.ToString() != origin) {
      return nullptr;
    }
    return resources_;
  }

  bool CanDownload(CefRefPtr<CefBrowser> browser, const CefString& url,
                   const CefString& request_method) override {
    return false;
  }

  bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                      const CefString& requesting_origin,
                                      uint32_t requested_permissions,
                                      CefRefPtr<CefMediaAccessCallback> callback) override {
    callback->Continue(CEF_MEDIA_PERMISSION_NONE);
    return true;
  }

  bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id,
                              const CefString& requesting_origin, uint32_t requested_permissions,
                              CefRefPtr<CefPermissionPromptCallback> callback) override {
    callback->Continue(CEF_PERMISSION_RESULT_DENY);
    return true;
  }

  void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefContextMenuParams> params,
                           CefRefPtr<CefMenuModel> model) override {
    model->Clear();  // No ambient external navigation or inspection UI.
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    CEF_REQUIRE_UI_THREAD();
    if (source_process != PID_RENDERER) {
      return false;
    }
    const bool handled = router_->OnProcessMessageReceived(browser, frame, source_process, message);
    if (smoke_test_ &&
        router_->GetPendingCount(nullptr, nullptr) > static_cast<int>(kMaxPendingQueries)) {
      FinishSmoke(false, "Native pending-query bound exceeded");
    }
    return handled;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64_t query_id,
               const CefString& request, bool persistent, CefRefPtr<Callback> callback) override {
    CEF_REQUIRE_UI_THREAD();
    if (router_->GetPendingCount(nullptr, nullptr) >= static_cast<int>(kMaxPendingQueries)) {
      // Returning true, even after Failure(), would register another pending
      // query until CEF's posted completion task runs. Do not admit it: false
      // triggers the router's immediate -1 rejection without storing QueryInfo.
      return false;
    }
    const auto reject = [&callback](int code, const char* message) {
      callback->Failure(code, message);
      return true;
    };
    // Authority comes from CEF's sender, not a renderer-supplied identity.
    if (closing_ || !browser_ || !browser_->IsSame(browser) || !frame || !frame->IsMain() ||
        !IsTrustedDocument(frame->GetURL().ToString())) {
      return reject(403, "Untrusted host-channel sender");
    }
    if (persistent) {
      return reject(400, "Persistent requests are not supported");
    }
    if (request.length() > kMaxRequestBytes) {
      return reject(413, "Request exceeds the host-channel limit");
    }
    const std::string json = request.ToString();
    if (json.size() > kMaxRequestBytes) {
      return reject(413, "Request exceeds the host-channel limit");
    }
    const auto value = CefParseJSON(json.data(), json.size(), JSON_PARSER_RFC);
    if (!value || value->GetType() != VTYPE_DICTIONARY) {
      return reject(400, "Request must be a JSON object");
    }
    const auto envelope = value->GetDictionary();
    if (envelope->GetSize() != 5 || envelope->GetType("protocol") != VTYPE_STRING ||
        envelope->GetType("version") != VTYPE_INT || envelope->GetType("id") != VTYPE_STRING ||
        envelope->GetType("method") != VTYPE_STRING ||
        envelope->GetType("params") != VTYPE_DICTIONARY) {
      return reject(400, "Malformed host-channel envelope");
    }
    const std::string protocol = envelope->GetString("protocol").ToString();
    const std::string id = envelope->GetString("id").ToString();
    const std::string method = envelope->GetString("method").ToString();
    const auto params = envelope->GetDictionary("params");
    const bool has_token = params->HasKey("token");
    if ((has_token && (params->GetSize() != 1 || params->GetType("token") != VTYPE_STRING)) ||
        (!has_token && params->GetSize() != 0)) {
      return reject(400, "Malformed host-channel parameters");
    }
    const std::string token = has_token ? params->GetString("token").ToString() : "";
    const HostRequest typed_request{
        protocol, envelope->GetInt("version"), id, method,
        has_token ? std::optional<std::string_view>(token) : std::nullopt};
    if (ValidateHostRequest(typed_request) != RequestError::kNone) {
      return reject(400, "Unsupported or invalid host-channel request");
    }

    const auto result = CefDictionaryValue::Create();
    if (*ParseHostMethod(method) == HostMethod::kDescribe) {
      result->SetString("name", "Kunlun Desktop");
      result->SetString("version", "0.1.0-dev");
      result->SetString("backend", "cef");
      result->SetString("cefVersion", CEF_VERSION);
      result->SetString("chromiumVersion", std::to_string(CHROME_VERSION_MAJOR) + "." +
                                               std::to_string(CHROME_VERSION_MINOR) + "." +
                                               std::to_string(CHROME_VERSION_BUILD) + "." +
                                               std::to_string(CHROME_VERSION_PATCH));
      result->SetBool("sandboxRequested", true);
      result->SetBool("sandboxQualified", false);
      result->SetBool("runtimeConnected", false);
    } else {
      if (ping_sequence_ == INT_MAX) {
        return reject(429, "Ping sequence exhausted; restart the preview");
      }
      result->SetString("token", token);
      result->SetInt("sequence", ++ping_sequence_);
    }
    const auto response = CefDictionaryValue::Create();
    response->SetString("protocol", std::string(kHostProtocol));
    response->SetInt("version", kHostProtocolVersion);
    response->SetString("id", id);
    response->SetDictionary("result", result);
    const auto response_value = CefValue::Create();
    response_value->SetDictionary(response);
    const std::string encoded = CefWriteJSON(response_value, JSON_WRITER_DEFAULT).ToString();
    if (encoded.empty() || encoded.size() > kMaxResponseBytes) {
      return reject(413, "Response exceeds the host-channel limit");
    }
    // Diagnostics compute synchronously with no application/service queue.
    // CEF retains this admitted callback until its posted completion task runs;
    // admission above bounds that router registry, including rejection replies.
    callback->Success(encoded);
    return true;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                 int http_status_code) override {
    CEF_REQUIRE_UI_THREAD();
    if (!smoke_test_ || smoke_started_ || !frame->IsMain()) {
      return;
    }
    smoke_started_ = true;
    if (http_status_code != 200 || !IsTrustedDocument(frame->GetURL().ToString())) {
      FinishSmoke(false, "Document did not load from the bundle");
      return;
    }
    frame->ExecuteJavaScript(kNativeSmokeScript, std::string(kDocumentUrl), 0);
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode error_code,
                   const CefString& error_text, const CefString& failed_url) override {
    if (frame->IsMain() && error_code != ERR_ABORTED) {
      std::fputs("Kunlun Desktop: bundled document failed to load\n", stderr);
      if (smoke_test_) {
        FinishSmoke(false, "Bundled document failed to load");
      }
    }
  }

  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                        const CefString& message, const CefString& source, int line) override {
    CEF_REQUIRE_UI_THREAD();
    const auto text = message.ToString();
    if (smoke_test_ && smoke_started_ && IsTrustedDocument(source.ToString())) {
      if (text == "KUNLUN_NATIVE_SMOKE_PASS") {
        FinishSmoke(true, "secure-origin assets, IPC, policy and UI interaction");
        return true;
      }
      if (text.rfind("KUNLUN_NATIVE_SMOKE_FAIL:", 0) == 0) {
        FinishSmoke(false, text.substr(0, 280));
        return true;
      }
    }
    if (level >= LOGSEVERITY_WARNING) {
      std::fprintf(stderr, "Presentation: %s\n", text.substr(0, 280).c_str());
    }
    return true;
  }

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status,
                                 int error_code, const CefString& error_string) override {
    CEF_REQUIRE_UI_THREAD();
    router_->OnRenderProcessTerminated(browser);
    std::fputs("Kunlun Desktop: renderer terminated; D2 recovery is not implemented\n", stderr);
    g_exit_code = 1;
    Close();
  }

  void Close() {
    CEF_REQUIRE_UI_THREAD();
    closing_ = true;
    if (browser_) {
      browser_->GetHost()->CloseBrowser(true);
    }
  }

  void FinishSmoke(bool success, std::string_view detail) {
    CEF_REQUIRE_UI_THREAD();
    if (!smoke_test_ || smoke_finished_) {
      return;
    }
    smoke_finished_ = true;
    g_exit_code = success ? 0 : 1;
    std::fprintf(stderr, "Kunlun native smoke %s: %.*s\n", success ? "PASS" : "FAIL",
                 static_cast<int>(detail.size()), detail.data());
    Close();
  }

 private:
  const bool smoke_test_;
  bool closing_ = false;
  bool smoke_started_ = false;
  bool smoke_finished_ = false;
  int ping_sequence_ = 0;
  CefRefPtr<CefBrowser> browser_;
  const CefRefPtr<LockedResourceHandler> resources_;
  const CefRefPtr<CefMessageRouterBrowserSide> router_;

  IMPLEMENT_REFCOUNTING(DesktopClient);
};

void SmokeDeadline::Execute() {
  CEF_REQUIRE_UI_THREAD();
  if (g_client) {
    g_client->FinishSmoke(false, "15-second native smoke deadline exceeded");
  }
}

}  // namespace

CefRefPtr<CefClient> CreateDesktopClient(bool smoke_test) { return new DesktopClient(smoke_test); }

void CloseDesktopWindow() {
  CEF_REQUIRE_UI_THREAD();
  if (g_client) {
    g_client->Close();
  }
}

int DesktopExitCode() { return g_exit_code; }

}  // namespace kunlun
