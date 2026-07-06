#ifndef WEBVIEW_PLATFORM_VIEW_FACTORY_H_
#define WEBVIEW_PLATFORM_VIEW_FACTORY_H_

#include <flutter_linux/flutter_linux.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(WebViewPlatformViewFactory,
                     webview_platform_view_factory,
                     WEBVIEW,
                     PLATFORM_VIEW_FACTORY,
                     FlPlatformViewFactory)

WebViewPlatformViewFactory *webview_platform_view_factory_new(
    FlMethodChannel *method_channel);

G_END_DECLS

#endif // WEBVIEW_PLATFORM_VIEW_FACTORY_H_
