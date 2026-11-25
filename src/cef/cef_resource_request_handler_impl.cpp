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
    const QUrl parsedUrl(requestUrl);
    const QString requestHost = parsedUrl.host().toLower();
    const int requestPort = parsedUrl.port(-1);
    const int allowedPort = DesktopAuthManager::instance().allowedPort();
    if (requestHost != allowedHost) {
        return RV_CONTINUE;
    }
    if (allowedPort > 0 && requestPort > 0 && requestPort != allowedPort) {
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
    const int effectivePort = parsedUrl.port(parsedUrl.scheme() == QStringLiteral("https") ? 443 : 80);
    if (parsedUrl.path().endsWith(QStringLiteral("/student/ks/strictExamEntry"))
            && (allowedPort <= 0 || effectivePort == allowedPort)) {
        headers.erase("X-Exam-Seat-Release");
        const QString releaseId = DesktopAuthManager::instance().examSeatReleaseId();
        if (!releaseId.isEmpty()) {
            headers.insert(std::make_pair("X-Exam-Seat-Release", releaseId.toStdString()));
        }
    }
    request->SetHeaderMap(headers);
    return RV_CONTINUE;
}
