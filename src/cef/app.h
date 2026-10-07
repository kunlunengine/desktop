#ifndef KUNLUN_DESKTOP_CEF_APP_H_
#define KUNLUN_DESKTOP_CEF_APP_H_

#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

namespace kunlun {

CefMessageRouterConfig HostRouterConfig();
bool VerifyLoadedCef();
CefRefPtr<CefApp> CreateDesktopApp(bool smoke_test = false);

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_CEF_APP_H_
