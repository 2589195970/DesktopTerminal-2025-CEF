#ifndef CEF_RESOURCE_REQUEST_HANDLER_IMPL_H
#define CEF_RESOURCE_REQUEST_HANDLER_IMPL_H

#include "include/cef_resource_request_handler.h"
#include "include/cef_version.h"

class CEFResourceRequestHandlerImpl : public CefResourceRequestHandler
{
public:
    cef_return_value_t OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
#if CEF_VERSION_MAJOR >= 109
        CefRefPtr<CefCallback> callback) override;
#else
        CefRefPtr<CefRequestCallback> callback) override;
#endif

private:
    IMPLEMENT_REFCOUNTING(CEFResourceRequestHandlerImpl);
};

#endif
