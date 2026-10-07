#include "host/policy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void Expect(bool condition, std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void TestOriginPolicy() {
  Expect(kunlun::IsTrustedDocument(kunlun::kDocumentUrl), "bundled document");
  Expect(kunlun::IsTrustedDocument(kunlun::kOrigin), "origin root");
  Expect(kunlun::IsTrustedDocument("kunlun://desktop/index.html#diagnostics"),
         "same-document fragment");
  for (const auto url : {
           "https://desktop/index.html",
           "file:///index.html",
           "http://localhost/index.html",
           "kunlun://desktop.evil/index.html",
           "kunlun://desktop@evil/index.html",
           "kunlun://evil@desktop/index.html",
           "kunlun://desktop:443/index.html",
           "kunlun://desktop/index.html?x=1",
           "kunlun://desktop/app.js",
           "kunlun://desktop/%69ndex.html",
           "kunlun://desktop/../index.html",
           "kunlun://desktop\\index.html",
           "KUNLUN://desktop/index.html",
           "about:blank",
           "data:text/html,hello",
       }) {
    Expect(!kunlun::IsTrustedDocument(url), url);
  }
  Expect(kunlun::AppResourcePath(kunlun::kOrigin) == "index.html", "root asset");
  Expect(kunlun::AppResourcePath("kunlun://desktop/app.css") == "app.css", "canonical resource");
  Expect(kunlun::AppResourcePath("kunlun://desktop/bridge.js#x") == "bridge.js",
         "resource fragment");
  for (const auto url : {
           "kunlun://desktop/../../secret",
           "kunlun://desktop/%2e%2e/secret",
           "kunlun://desktop//app.js",
           "kunlun://desktop/.hidden",
           "kunlun://desktop/app.js?redirect=https://evil",
           "kunlun://desktop/app.js/extra",
           "kunlun://desktop/APP.js",
           "kunlun://desktop/a..js",
           "kunlun://desktop/a\\b.js",
           "kunlun://other/app.js",
           "https://desktop/app.js",
       }) {
    Expect(!kunlun::AppResourcePath(url), url);
  }
}

void TestLaunchPolicy() {
  Expect(kunlun::IsAllowedBrowserArgument("--smoke-test"), "smoke test switch");
  for (const auto argument : {
           "--no-sandbox",
           "--disable-gpu-sandbox",
           "--single-process",
           "--disable-web-security",
           "--remote-debugging-port=9222",
           "--browser-subprocess-path=/tmp/helper",
           "--disable-features=Sandbox",
           "--user-data-dir=/tmp/profile",
           "--url=https://evil",
           "https://evil",
           "--smoke-test=1",
           "--unknown",
       }) {
    Expect(!kunlun::IsAllowedBrowserArgument(argument), argument);
  }
}

void TestHostProtocol() {
  using kunlun::RequestError;
  kunlun::HostRequest request{kunlun::kHostProtocol, 1, "request_1", "host.describe", std::nullopt};
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kNone, "describe");
  request.protocol = "kunlun.runtime-provider/v0.2";
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kProtocol,
         "runtime protocol cannot invoke host diagnostics");
  request.protocol = kunlun::kHostProtocol;
  for (const int version : {-1, 0, 2, 100}) {
    request.version = version;
    Expect(kunlun::ValidateHostRequest(request) == RequestError::kVersion,
           "reject unsupported protocol version");
  }
  request.version = 1;
  const std::string long_id(65, 'a');
  for (const std::string_view id :
       {std::string_view{}, std::string_view{"a/b"}, std::string_view{"a b"},
        std::string_view{"\xc3\xa9"}, std::string_view{long_id}}) {
    request.id = id;
    Expect(kunlun::ValidateHostRequest(request) == RequestError::kId,
           "reject invalid request identity");
  }
  const std::string maximum_id(64, 'a');
  request.id = maximum_id;
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kNone, "maximum request identity");
  for (const auto method : {"app.eval", "host.open", "host.describe ", ""}) {
    request.method = method;
    Expect(kunlun::ValidateHostRequest(request) == RequestError::kMethod, method);
  }
  request.method = "host.describe";
  request.token = "extra";
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kParams,
         "describe does not take a token");
  request.method = "host.ping";
  request.token.reset();
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kParams, "ping requires token");
  request.token = "";
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kNone, "empty ping token");
  const std::string maximum_token(128, '~');
  request.token = maximum_token;
  Expect(kunlun::ValidateHostRequest(request) == RequestError::kNone, "maximum ping token");
  const std::string long_token(129, 'a');
  for (const std::string_view token :
       {std::string_view{"\n"}, std::string_view{"\x7f"}, std::string_view{"\xc3\xa9"},
        std::string_view{long_token}}) {
    request.token = token;
    Expect(kunlun::ValidateHostRequest(request) == RequestError::kParams,
           "reject invalid ping token");
  }
}

}  // namespace

int main() {
  TestOriginPolicy();
  TestLaunchPolicy();
  TestHostProtocol();
  if (failures) {
    std::cerr << failures << " policy checks failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Host policy checks passed\n";
  return EXIT_SUCCESS;
}
