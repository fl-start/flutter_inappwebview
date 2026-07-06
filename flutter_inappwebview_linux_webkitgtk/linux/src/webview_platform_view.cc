// Platform view implementation for embedding WebKitWebView
#include "webview_platform_view.h"
#include "webview_webkitgtk.h"
#include <gtk/gtk.h>

#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define g_print(...) ((void)0)
#endif

WebViewPlatformView *webview_platform_view_new(
    FlMethodChannel *method_channel,
    gint64 view_id)
{
  WebViewPlatformView *instance =
      g_new0(WebViewPlatformView, 1);
  instance->method_channel = method_channel;
  instance->view_id = view_id;

  // Create a GTK container (event box) to hold the WebView
  instance->container = gtk_event_box_new();
  gtk_widget_set_hexpand(instance->container, TRUE);
  gtk_widget_set_vexpand(instance->container, TRUE);

  // Create the WebKitWebView instance
  instance->webkit_view = webview_webkitgtk_new(method_channel, view_id, nullptr);

  // Get the WebKitWebView GTK widget
  GtkWidget *web_view_widget =
      webview_webkitgtk_get_widget(instance->webkit_view);

  if (web_view_widget)
  {
    // Add the WebView to the container
    gtk_container_add(GTK_CONTAINER(instance->container), web_view_widget);
    gtk_widget_show_all(instance->container);
    g_print("🐧 Platform view created (view_id: %ld)\n", view_id);
  }
  else
  {
    g_warning("🐧 Failed to get WebKitWebView widget\n");
  }

  return instance;
}

void webview_platform_view_destroy(
    WebViewPlatformView *instance)
{
  if (!instance)
    return;

  g_print("🐧 Destroying platform view (view_id: %ld)\n", instance->view_id);

  if (instance->webkit_view)
  {
    webview_webkitgtk_destroy(instance->webkit_view);
  }

  if (instance->container)
  {
    gtk_widget_destroy(instance->container);
  }

  g_free(instance);
}

GtkWidget *webview_platform_view_get_widget(
    WebViewPlatformView *instance)
{
  if (!instance)
    return nullptr;
  return instance->container;
}
