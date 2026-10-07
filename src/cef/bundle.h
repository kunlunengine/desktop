#ifndef KUNLUN_DESKTOP_CEF_BUNDLE_H_
#define KUNLUN_DESKTOP_CEF_BUNDLE_H_

#include <string_view>

#include "host/asset.h"
#include "include/cef_resource_handler.h"

namespace kunlun {

const Asset* FindBundledAsset(std::string_view url);
CefRefPtr<CefResourceHandler> CreateBundleResource(std::string_view url);

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_CEF_BUNDLE_H_
