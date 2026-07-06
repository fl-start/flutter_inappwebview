#ifndef WEBVIEW_PLATFORM_VIEW_H_
#define WEBVIEW_PLATFORM_VIEW_H_

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include "webview_webkitgtk.h"

G_BEGIN_DECLS

// Platform view for embedding WebKitWebView in Flutter
// On Linux, we use a GTK container widget to embed the WebKitWebView

typedef struct _WebViewPlatformView WebViewPlatformView;

struct _WebViewPlatformView
{
    GtkWidget *container;
    WebViewWebKitGTK *webkit_view;
    FlMethodChannel *method_channel;
    gint64 view_id;
};

// Create a platform view instance
WebViewPlatformView *webview_platform_view_new(
    FlMethodChannel *method_channel,
    gint64 view_id);

// Destroy platform view instance
void webview_platform_view_destroy(
    WebViewPlatformView *instance);

// Get the GTK widget for embedding
GtkWidget *webview_platform_view_get_widget(
    WebViewPlatformView *instance);

G_END_DECLS

#endif // WEBVIEW_PLATFORM_VIEW_H_
