#ifndef KUNLUN_DESKTOP_HOST_ASSET_H_
#define KUNLUN_DESKTOP_HOST_ASSET_H_

#include <string_view>

namespace kunlun {

struct Asset {
  std::string_view path;
  std::string_view mime_type;
  std::string_view content;
};

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_HOST_ASSET_H_
