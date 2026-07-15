// WebView Plugin - Main plugin registration
// This file handles plugin registration with Flutter

#include "webview_plugin.h"
#include <flutter_inappwebview_linux_webkitgtk/flutter_inappwebview_linux_webkitgtk_plugin.h>
#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h> // For GHashTable
#include "webview_webkitgtk.h"
#include "webview_overlay_window.h"
#include "webview_plugin_methods.h"
#include "webview_plugin_lifecycle_handlers.h"
#include "webview_plugin_content_handlers.h"

#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define g_print(...) ((void)0)
#endif

#define WEBVIEW_PLUGIN(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), webview_plugin_get_type(), \
                              WebViewPlugin))

struct _WebViewPlugin
{
  GObject parent_instance;
  FlMethodChannel *method_channel;
  FlPluginRegistrar *registrar; // Store registrar to get parent window
  WebKitWebContext *shared_web_context;
  // Hash table of view_id -> overlay window instance (gint64 -> WebViewOverlayWindow*)
  GHashTable *overlay_windows;
};

G_DEFINE_TYPE(WebViewPlugin, webview_plugin, g_object_get_type())

// Hash table helpers for gint64 keys
static guint platform_view_hash(gconstpointer key)
{
  return (guint)(GPOINTER_TO_SIZE(key));
}

static gboolean platform_view_equal(gconstpointer a, gconstpointer b)
{
  return GPOINTER_TO_SIZE(a) == GPOINTER_TO_SIZE(b);
}

// Called when a method call is received from Flutter.
static void webview_plugin_handle_method_call(
    WebViewPlugin *self,
    FlMethodCall *method_call)
{
  g_autoptr(FlMethodResponse) response = nullptr;

  const gchar *method = fl_method_call_get_name(method_call);
  FlValue *args = fl_method_call_get_args(method_call);

  g_print("📱 WebView: Method call: %s\n", method);

  FlMethodResponse *lifecycle_response = nullptr;
  if (webview_plugin_try_handle_lifecycle_method(
          self->method_channel,
          self->registrar,
          self->shared_web_context,
          self->overlay_windows,
          method,
          args,
          method_call,
          &lifecycle_response))
  {
    if (lifecycle_response != nullptr)
    {
      fl_method_call_respond(method_call, lifecycle_response, nullptr);
    }
    return;
  }

  FlMethodResponse *content_response = nullptr;
  if (webview_plugin_try_handle_content_method(
          self->overlay_windows, method, args, method_call, &content_response))
  {
    if (content_response != nullptr)
    {
      fl_method_call_respond(method_call, content_response, nullptr);
    }
    return;
  }
  {
    // Unknown method
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  // Only respond if response was set (evaluateJavaScript handles its own response asynchronously)
  if (response != nullptr)
  {
    fl_method_call_respond(method_call, response, nullptr);
  }
}

// Helper function for destroying overlay windows in hash table
static void destroy_overlay_window(gpointer key, gpointer value, gpointer user_data)
{
  WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)value;
  webview_overlay_window_destroy(overlay_window);
}

static void webview_plugin_dispose(GObject *object)
{
  WebViewPlugin *self = WEBVIEW_PLUGIN(object);

  // Clean up all overlay windows
  if (self->overlay_windows)
  {
    g_hash_table_foreach(self->overlay_windows, destroy_overlay_window, nullptr);
    g_hash_table_destroy(self->overlay_windows);
    self->overlay_windows = nullptr;
  }

  if (self->method_channel)
  {
    g_object_unref(self->method_channel);
    self->method_channel = nullptr;
  }

  if (self->registrar)
  {
    g_object_unref(self->registrar);
    self->registrar = nullptr;
  }

  if (self->shared_web_context)
  {
    g_object_unref(self->shared_web_context);
    self->shared_web_context = nullptr;
  }

  G_OBJECT_CLASS(webview_plugin_parent_class)->dispose(object);
}

static void webview_plugin_class_init(WebViewPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->dispose = webview_plugin_dispose;
}

static void webview_plugin_init(WebViewPlugin *self)
{
  self->method_channel = nullptr;
  self->registrar = nullptr;
  self->shared_web_context = webkit_web_context_new();
  // Create hash table for overlay windows (key: gint64 as gpointer, value: WebViewOverlayWindow*)
  self->overlay_windows = g_hash_table_new_full(
      platform_view_hash, // Reusing hash/equal functions (they work for gint64 keys)
      platform_view_equal,
      nullptr,  // key destroy function (not needed, we store pointer to integer)
      nullptr); // value destroy function (handled manually in dispose)
}

static void method_call_cb(FlMethodChannel *channel, FlMethodCall *method_call,
                           gpointer user_data)
{
  WebViewPlugin *plugin = WEBVIEW_PLUGIN(user_data);
  webview_plugin_handle_method_call(plugin, method_call);
}

void webview_plugin_register_with_registrar(FlPluginRegistrar *registrar)
{
  WebViewPlugin *plugin = WEBVIEW_PLUGIN(
      g_object_new(webview_plugin_get_type(), nullptr));

  // Create method channel for Dart ↔ C++ communication
  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  FlMethodChannel *channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "webview_webkitgtk",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);

  // Store channel and registrar references in plugin
  plugin->method_channel = g_object_ref(channel);
  plugin->registrar = g_object_ref(registrar);

  g_print("✓ WebView plugin registered (overlay window mode)\n");

  g_object_unref(plugin);
}

GType flutter_inappwebview_linux_webkitgtk_plugin_get_type() {
  return webview_plugin_get_type();
}

void flutter_inappwebview_linux_webkitgtk_plugin_register_with_registrar(
    FlPluginRegistrar *registrar) {
  webview_plugin_register_with_registrar(registrar);
}
