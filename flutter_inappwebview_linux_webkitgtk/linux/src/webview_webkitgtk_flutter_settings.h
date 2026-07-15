#ifndef WEBVIEW_WEBKITGTK_FLUTTER_SETTINGS_H_
#define WEBVIEW_WEBKITGTK_FLUTTER_SETTINGS_H_

#include <flutter_linux/flutter_linux.h>
#include <webkit2/webkit2.h>

struct _WebViewWebKitGTK;
typedef struct _WebViewWebKitGTK WebViewWebKitGTK;

// Generic embedder contract: optional Flutter StandardCodec map with bool/int keys.
// Unknown keys are ignored. No app-specific types or imports.

WebKitWebContext *webview_webkitgtk_create_context_from_flutter_settings(
    FlValue *settings_map_or_null,
    WebKitWebContext *shared_context_or_null);

void webview_webkitgtk_apply_flutter_settings_map(
    WebViewWebKitGTK *instance,
    FlValue *settings_map_or_null);

void webview_webkitgtk_flutter_settings_install_handlers(WebViewWebKitGTK *instance);

#endif // WEBVIEW_WEBKITGTK_FLUTTER_SETTINGS_H_
