#include "webview_platform_view_factory.h"
#include "webview_platform_view.h"
#include <flutter_linux/flutter_linux.h>

#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define g_print(...) ((void)0)
#endif

struct _WebViewPlatformViewFactory
{
    FlPlatformViewFactory parent_instance;
    FlMethodChannel *method_channel;
};

G_DEFINE_TYPE(WebViewPlatformViewFactory,
              webview_platform_view_factory,
              fl_platform_view_factory_get_type())

static FlPlatformView *create_platform_view(
    FlPlatformViewFactory *factory,
    int64_t view_id,
    FlValue *args,
    FlBinaryMessenger *messenger)
{
    WebViewPlatformViewFactory *self =
        WEBVIEW_PLATFORM_VIEW_FACTORY(factory);

    // Extract viewId from args (should match the one passed from Dart)
    gint64 view_id_from_args = view_id; // Flutter passes view_id as int64_t

    // Create platform view
    WebViewPlatformView *platform_view =
        webview_platform_view_new(self->method_channel, view_id_from_args);

    // Get the GTK widget
    GtkWidget *widget = webview_platform_view_get_widget(platform_view);

    // Create FlPlatformView wrapper
    FlPlatformView *fl_platform_view = fl_platform_view_new(
        FL_PLATFORM_VIEW_IMPL(widget), // The GTK widget as platform view impl
        fl_platform_view_implements_gesture_delegator_new());

    g_print("🐧 Platform view factory created view (view_id: %ld)\n", view_id_from_args);

    return fl_platform_view;
}

static void webview_platform_view_factory_class_init(
    WebViewPlatformViewFactoryClass *klass)
{
    FL_PLATFORM_VIEW_FACTORY_CLASS(klass)->create_platform_view =
        create_platform_view;
}

static void webview_platform_view_factory_init(
    WebViewPlatformViewFactory *self)
{
    self->method_channel = nullptr;
}

WebViewPlatformViewFactory *webview_platform_view_factory_new(
    FlMethodChannel *method_channel)
{
    WebViewPlatformViewFactory *factory =
        WEBVIEW_PLATFORM_VIEW_FACTORY(
            g_object_new(webview_platform_view_factory_get_type(), nullptr));
    factory->method_channel = method_channel;
    return factory;
}
