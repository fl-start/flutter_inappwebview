#ifndef WEBVIEW_WEBKITGTK_H_
#define WEBVIEW_WEBKITGTK_H_

#include <webkit2/webkit2.h>
#include <gtk/gtk.h>
#include <flutter_linux/flutter_linux.h>

G_BEGIN_DECLS

// WebKitGTK WebView wrapper
// This class manages a WebKitWebView instance and handles communication
// with Flutter via method channels

typedef struct _WebViewWebKitGTK WebViewWebKitGTK;

// Structure to store content for custom scheme routes
typedef struct
{
    gchar *content;       // Content (can be text or binary, stored as bytes)
    gsize content_length; // Length of content
    gchar *content_type;  // MIME type (e.g., "text/html", "image/png")
    gboolean is_text;     // TRUE for text content, FALSE for binary
} WebViewSchemeContent;

struct _WebViewWebKitGTK
{
    WebKitWebView *web_view;
    WebKitWebContext *web_context;
    WebKitUserContentManager *user_content_manager;
    FlMethodChannel *method_channel;
    gint64 view_id;            // Platform view ID for Flutter
    GHashTable *scheme_routes; // Hash table: path (gchar*) -> WebViewSchemeContent*
    GHashTable *script_handlers; // handler name (gchar*) -> ScriptHandlerUserData*
    // Generic Flutter "settings" map policy flags (updated by apply + initial create).
    gboolean flutter_block_network_loads;
    gboolean flutter_allow_file_access;
    gboolean flutter_geolocation_enabled;
    /// Restored after each load-finished (matches prior fixed-zoom behavior unless changed).
    gdouble zoom_level_after_load;
};

// Create a new WebKitGTK WebView instance
// [settings_map_or_null] optional Flutter Standard map (bools/ints); may be null.
WebViewWebKitGTK *webview_webkitgtk_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlValue *settings_map_or_null,
    WebKitWebContext *shared_context_or_null);

// Destroy WebKitGTK WebView instance
void webview_webkitgtk_destroy(WebViewWebKitGTK *instance);

// Get the GTK widget (for platform view embedding)
GtkWidget *webview_webkitgtk_get_widget(WebViewWebKitGTK *instance);

// Load URL
void webview_webkitgtk_load_url(WebViewWebKitGTK *instance,
                                const gchar *url,
                                FlValue *headers_map_or_null);

// Load HTML
void webview_webkitgtk_load_html(WebViewWebKitGTK *instance,
                                 const gchar *html,
                                 const gchar *base_url);

// Evaluate JavaScript
void webview_webkitgtk_evaluate_javascript(
    WebViewWebKitGTK *instance,
    const gchar *javascript,
    FlMethodCall *method_call);

// Reload
void webview_webkitgtk_reload(WebViewWebKitGTK *instance);

// Go back
void webview_webkitgtk_go_back(WebViewWebKitGTK *instance);

// Check if can go back
gboolean webview_webkitgtk_can_go_back(WebViewWebKitGTK *instance);

// Get current URL
gchar *webview_webkitgtk_get_current_url(WebViewWebKitGTK *instance);

// Register custom scheme handler (appmsg://)
void webview_webkitgtk_register_custom_scheme(
    WebViewWebKitGTK *instance,
    const gchar *scheme);

// Register a route for the custom scheme
void webview_webkitgtk_register_scheme_route(
    WebViewWebKitGTK *instance,
    const gchar *path,
    const gchar *content,
    gsize content_length,
    const gchar *content_type,
    gboolean is_text);

// Unregister a scheme route
void webview_webkitgtk_unregister_scheme_route(
    WebViewWebKitGTK *instance,
    const gchar *path);

// Register JavaScript message handler
void webview_webkitgtk_register_message_handler(
    WebViewWebKitGTK *instance,
    const gchar *handler_name);

// Set zoom level (1.0 = 100%, 1.5 = 150%, etc.)
void webview_webkitgtk_set_zoom_level(
    WebViewWebKitGTK *instance,
    double zoom_level);

// Inject UserScript list from Flutter StandardCodec (list of maps with source + injectionTime).
void webview_webkitgtk_add_user_scripts_from_flvalue(
    WebViewWebKitGTK *instance,
    FlValue *scripts_list_or_null);

G_END_DECLS

#endif // WEBVIEW_WEBKITGTK_H_
