#ifndef WEBVIEW_OVERLAY_WINDOW_H_
#define WEBVIEW_OVERLAY_WINDOW_H_

#include <flutter_linux/flutter_linux.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "webview_webkitgtk.h"

G_BEGIN_DECLS

typedef struct _WebViewOverlayWindow WebViewOverlayWindow;

typedef enum
{
    WEBVIEW_WINDOW_MODE_OVERLAY,
    WEBVIEW_WINDOW_MODE_SEPARATE
} WebViewWindowMode;

struct _WebViewOverlayWindow
{
    GtkWindow *window;
    GtkWidget *container;
    WebViewWebKitGTK *webkit_view;
    FlMethodChannel *method_channel;
    gint64 view_id;
    gint x;
    gint y;
    gint width;
    gint height;
    GtkWindow *parent_window;
    FlView *flutter_view;
    gulong parent_configure_handler_id;
    WebViewWindowMode window_mode;
    gboolean embedded_widget_mode;
    GtkWidget *embedding_overlay;
    gulong child_position_handler_id;
    gulong host_layout_flutter_handler_id;
    gulong host_layout_overlay_handler_id;
    // Embedded GtkOverlay-relative allocation (not screen coordinates).
    gboolean has_applied_overlay_bounds;
    gint last_overlay_alloc_x;
    gint last_overlay_alloc_y;
    gint last_overlay_alloc_width;
    gint last_overlay_alloc_height;
    // Popup / separate-window screen coordinates.
    gboolean has_applied_screen_bounds;
    gint last_screen_x;
    gint last_screen_y;
    gint last_screen_width;
    gint last_screen_height;
    gboolean has_reported_host_alloc;
    gint last_reported_host_alloc_width;
    gint last_reported_host_alloc_height;
    gboolean has_reported_parent_configure;
    gint last_reported_parent_configure_width;
    gint last_reported_parent_configure_height;
    gint64 last_bounds_sequence;
    guint idle_force_bounds_source_id;
    guint timeout_force_bounds_32_source_id;
    guint timeout_force_bounds_120_source_id;
    guint idle_raise_source_id;
    guint timeout_raise_32_source_id;
};

WebViewOverlayWindow *webview_overlay_window_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlView *flutter_view,
    WebViewWindowMode window_mode,
    FlValue *initial_settings_map_or_null,
    WebKitWebContext *shared_context_or_null);

void webview_overlay_window_destroy(
    WebViewOverlayWindow *instance);

void webview_overlay_window_show(WebViewOverlayWindow *instance);

void webview_overlay_window_hide(WebViewOverlayWindow *instance);

void webview_overlay_window_hide_others(
    GHashTable *overlay_windows,
    WebViewOverlayWindow *keep);

void webview_overlay_window_set_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height);

void webview_overlay_window_set_bounds_from_flutter(
    WebViewOverlayWindow *instance,
    gdouble x,
    gdouble y,
    gdouble width,
    gdouble height,
    gdouble view_width,
    gdouble view_height,
    gdouble device_pixel_ratio,
    gint64 sequence);

void webview_overlay_window_set_bounds_screen(
    WebViewOverlayWindow *instance,
    gint screen_x,
    gint screen_y,
    gint width,
    gint height);

WebViewWebKitGTK *webview_overlay_window_get_webkit_view(
    WebViewOverlayWindow *instance);

void webview_overlay_window_grab_focus(WebViewOverlayWindow *instance);

void webview_overlay_window_release_focus(WebViewOverlayWindow *instance);

void webview_overlay_window_position_next_to_main(
    WebViewOverlayWindow *instance,
    gint width,
    gint height,
    gint main_x,
    gint main_y,
    gint main_width,
    gint main_height);

G_END_DECLS

#endif // WEBVIEW_OVERLAY_WINDOW_H_
