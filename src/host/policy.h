#ifndef KUNLUN_DESKTOP_HOST_POLICY_H_
#define KUNLUN_DESKTOP_HOST_POLICY_H_

#include <cstddef>
#include <optional>
#include <string_view>

namespace kunlun {

inline constexpr std::string_view kOrigin = "kunlun://desktop/";
inline constexpr std::string_view kDocumentUrl = "kunlun://desktop/index.html";
inline constexpr std::string_view kHostProtocol = "kunlun.desktop.host";
inline constexpr int kHostProtocolVersion = 1;
inline constexpr std::size_t kMaxRequestBytes = 4096;
inline constexpr std::size_t kMaxResponseBytes = 16384;
inline constexpr std::size_t kMaxPendingQueries = 16;
inline constexpr std::string_view kContentSecurityPolicy =
    "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self'; "
    "font-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; "
    "frame-ancestors 'none'; form-action 'none'";

// Only the bundled document may navigate or invoke the host bridge. Resource
// authorization also requires an exact entry in the embedded asset table.
bool IsTrustedDocument(std::string_view url);
std::optional<std::string_view> AppResourcePath(std::string_view url);

// The browser accepts no Chromium switches from the caller. Child processes
// receive CEF-generated arguments through a separate helper entry point.
bool IsAllowedBrowserArgument(std::string_view argument);

enum class HostMethod { kDescribe, kPing };
enum class RequestError {
  kNone,
  kProtocol,
  kVersion,
  kId,
  kMethod,
  kParams,
};

struct HostRequest {
  std::string_view protocol;
  int version;
  std::string_view id;
  std::string_view method;
  std::optional<std::string_view> token;
};

// Desktop-owned diagnostics only; this is not the runtime application protocol.
RequestError ValidateHostRequest(const HostRequest& request);
std::optional<HostMethod> ParseHostMethod(std::string_view method);

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_HOST_POLICY_H_
