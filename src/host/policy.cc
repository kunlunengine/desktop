#include "host/policy.h"

namespace kunlun {
namespace {

std::string_view WithoutFragment(std::string_view url) { return url.substr(0, url.find('#')); }

bool IsRequestId(std::string_view id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  for (const unsigned char character : id) {
    if (!(character >= 'a' && character <= 'z') && !(character >= 'A' && character <= 'Z') &&
        !(character >= '0' && character <= '9') && character != '_' && character != '-') {
      return false;
    }
  }
  return true;
}

bool IsPingToken(std::string_view token) {
  if (token.size() > 128) {
    return false;
  }
  for (const unsigned char character : token) {
    if (character < 0x20 || character > 0x7e) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool IsTrustedDocument(std::string_view url) {
  url = WithoutFragment(url);
  return url == kOrigin || url == kDocumentUrl;
}

std::optional<std::string_view> AppResourcePath(std::string_view url) {
  url = WithoutFragment(url);
  if (url.substr(0, kOrigin.size()) != kOrigin) {
    return std::nullopt;
  }
  auto path = url.substr(kOrigin.size());
  if (path.empty()) {
    return "index.html";
  }
  // This is a canonical, flat, immutable bundle, not a filesystem URL mapper.
  // Reject aliases before the caller checks the exact embedded asset table.
  for (const unsigned char character : path) {
    if (!(character >= 'a' && character <= 'z') && !(character >= '0' && character <= '9') &&
        character != '.' && character != '-') {
      return std::nullopt;
    }
  }
  if (path.front() == '.' || path.find("..") != std::string_view::npos) {
    return std::nullopt;
  }
  return path;
}

bool IsAllowedBrowserArgument(std::string_view argument) { return argument == "--smoke-test"; }

std::optional<HostMethod> ParseHostMethod(std::string_view method) {
  if (method == "host.describe") {
    return HostMethod::kDescribe;
  }
  if (method == "host.ping") {
    return HostMethod::kPing;
  }
  return std::nullopt;
}

RequestError ValidateHostRequest(const HostRequest& request) {
  if (request.protocol != kHostProtocol) {
    return RequestError::kProtocol;
  }
  if (request.version != kHostProtocolVersion) {
    return RequestError::kVersion;
  }
  if (!IsRequestId(request.id)) {
    return RequestError::kId;
  }
  const auto method = ParseHostMethod(request.method);
  if (!method) {
    return RequestError::kMethod;
  }
  if (*method == HostMethod::kDescribe) {
    return request.token ? RequestError::kParams : RequestError::kNone;
  }
  return request.token && IsPingToken(*request.token) ? RequestError::kNone : RequestError::kParams;
}

}  // namespace kunlun
