#ifndef KUNLUN_DESKTOP_CEF_CLIENT_H_
#define KUNLUN_DESKTOP_CEF_CLIENT_H_

#include "include/cef_client.h"

namespace kunlun {

CefRefPtr<CefClient> CreateDesktopClient(bool smoke_test);
void CloseDesktopWindow();
int DesktopExitCode();

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_CEF_CLIENT_H_
