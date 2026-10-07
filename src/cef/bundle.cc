#include "cef/bundle.h"

#include <algorithm>
#include <cstring>

#include "embedded_assets.h"
#include "host/policy.h"

namespace kunlun {
namespace {

class BundleResource final : public CefResourceHandler {
 public:
  explicit BundleResource(const Asset& asset) : asset_(asset) {}

  bool Open(CefRefPtr<CefRequest> request, bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    handle_request = true;
    return request->GetMethod() == "GET";
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length,
                          CefString& redirect_url) override {
    response->SetStatus(200);
    response->SetStatusText("OK");
    response->SetMimeType(std::string(asset_.mime_type));
    response->SetCharset("utf-8");
    CefResponse::HeaderMap headers;
    headers.emplace("Content-Security-Policy", std::string(kContentSecurityPolicy));
    headers.emplace("X-Content-Type-Options", "nosniff");
    headers.emplace("Cache-Control", "no-store");
    headers.emplace("Referrer-Policy", "no-referrer");
    headers.emplace("Permissions-Policy",
                    "camera=(), microphone=(), geolocation=(), usb=(), "
                    "serial=(), hid=(), clipboard-read=(), clipboard-write=(), "
                    "display-capture=()");
    response->SetHeaderMap(headers);
    response_length = static_cast<int64_t>(asset_.content.size());
  }

  bool Read(void* data_out, int bytes_to_read, int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    bytes_read = 0;
    if (bytes_to_read <= 0 || offset_ == asset_.content.size()) {
      return false;
    }
    const auto count =
        std::min(static_cast<std::size_t>(bytes_to_read), asset_.content.size() - offset_);
    std::memcpy(data_out, asset_.content.data() + offset_, count);
    offset_ += count;
    bytes_read = static_cast<int>(count);
    return true;
  }

  void Cancel() override {}

 private:
  const Asset& asset_;
  std::size_t offset_ = 0;

  IMPLEMENT_REFCOUNTING(BundleResource);
};

}  // namespace

const Asset* FindBundledAsset(std::string_view url) {
  const auto path = AppResourcePath(url);
  if (!path) {
    return nullptr;
  }
  for (const auto& asset : kAssets) {
    if (asset.path == *path) {
      return &asset;
    }
  }
  return nullptr;
}

CefRefPtr<CefResourceHandler> CreateBundleResource(std::string_view url) {
  const auto* asset = FindBundledAsset(url);
  return asset ? new BundleResource(*asset) : nullptr;
}

}  // namespace kunlun
