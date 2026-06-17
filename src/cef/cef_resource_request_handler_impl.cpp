#include "cef_resource_request_handler_impl.h"

#include "../network/desktop_auth_manager.h"

#include <QUrl>

cef_return_value_t CEFResourceRequestHandlerImpl::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefRequestCallback> callback)
{
    Q_UNUSED(browser)
    Q_UNUSED(frame)
    Q_UNUSED(callback)

    const QString allowedHost = DesktopAuthManager::instance().allowedHost();
    if (allowedHost.isEmpty()) {
        return RV_CONTINUE;
    }

    const QString requestUrl = QString::fromStdString(request->GetURL().ToString());
    const QString requestHost = QUrl(requestUrl).host().toLower();
    if (requestHost != allowedHost) {
        return RV_CONTINUE;
    }

    DesktopAuthManager::instance().refreshIfNeeded();
    const QString token = DesktopAuthManager::instance().buildRequestToken();
    if (token.isEmpty()) {
        return RV_CONTINUE;
    }

    CefRequest::HeaderMap headers;
    request->GetHeaderMap(headers);
    headers.insert(std::make_pair("X-Desktop-Token", token.toStdString()));
    request->SetHeaderMap(headers);
    return RV_CONTINUE;
}
