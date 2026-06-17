#ifndef CEF_RESOURCE_REQUEST_HANDLER_IMPL_H
#define CEF_RESOURCE_REQUEST_HANDLER_IMPL_H

#include "include/cef_resource_request_handler.h"

class CEFResourceRequestHandlerImpl : public CefResourceRequestHandler
{
public:
    cef_return_value_t OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefRequestCallback> callback) override;

private:
    IMPLEMENT_REFCOUNTING(CEFResourceRequestHandlerImpl);
};

#endif
