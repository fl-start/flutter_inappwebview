#ifndef WEBVIEW_PLUGIN_LIFECYCLE_HANDLERS_H_
#define WEBVIEW_PLUGIN_LIFECYCLE_HANDLERS_H_

#include <flutter_linux/flutter_linux.h>
#include <glib.h>

// Handles window lifecycle + placement methods.
// Returns true if the method was handled and sets out_response.
bool webview_plugin_try_handle_lifecycle_method(
    FlMethodChannel *method_channel,
    FlPluginRegistrar *registrar,
    GHashTable *overlay_windows,
    const gchar *method,
    FlValue *args,
    FlMethodCall *method_call,
    FlMethodResponse **out_response);

#endif // WEBVIEW_PLUGIN_LIFECYCLE_HANDLERS_H_
