#ifndef WEBVIEW_OVERLAY_WINDOW_H_
#define WEBVIEW_OVERLAY_WINDOW_H_

#include <flutter_linux/flutter_linux.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "webview_webkitgtk.h"

G_BEGIN_DECLS

// Overlay window for embedding WebKitWebView
// Creates a popup window positioned over the Flutter app
// Note: We use a popup window because Flutter Linux doesn't allow modifying the window structure
typedef struct _WebViewOverlayWindow WebViewOverlayWindow;

// Window mode enum
typedef enum
{
    WEBVIEW_WINDOW_MODE_OVERLAY, // Overlay popup window (default)
    WEBVIEW_WINDOW_MODE_SEPARATE // Separate top-level window (like thunderbird)
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
    GtkWindow *parent_window;           // Parent window reference
    FlView *flutter_view;               // Flutter view for getting client area position
    gulong parent_configure_handler_id; // Signal handler ID for parent window configure events
    WebViewWindowMode window_mode;      // Window mode (overlay or separate)
    gboolean embedded_widget_mode;      // true when attached as child of GtkOverlay
    GtkWidget *embedding_overlay;       // GtkOverlay that hosts Flutter view
    gulong child_position_handler_id;   // ID for "get-child-position" signal on embedding_overlay
    gulong host_layout_flutter_handler_id; // FlView size-allocate → Dart bounds pull
    gulong host_layout_overlay_handler_id; // GtkOverlay size-allocate → Dart bounds pull
    gboolean has_applied_screen_bounds;
    gint last_screen_x;
    gint last_screen_y;
    gint last_screen_width;
    gint last_screen_height;
    // Last host allocation actually reported to Dart via onHostLayoutChanged.
    // Guards against a feedback loop: applying (even unchanged) bounds can
    // call gtk_widget_queue_resize, which fires size-allocate again even when
    // the allocation doesn't change, which would otherwise re-notify Dart,
    // which re-applies bounds, forever. See on_host_size_allocate.
    gboolean has_reported_host_alloc;
    gint last_reported_host_alloc_width;
    gint last_reported_host_alloc_height;
    // Same idea, for on_parent_configure_event (window-level move/resize),
    // which is a distinct signal from the embedding_overlay's own
    // size-allocate and must not share its cache (they can legitimately
    // report different dimensions for the same moment).
    gboolean has_reported_parent_configure;
    gint last_reported_parent_configure_width;
    gint last_reported_parent_configure_height;
    // Monotonic setBounds sequence from Dart; ignore stale async updates.
    gint64 last_bounds_sequence;
};

// Create overlay window (returns to popup approach since we can't modify window structure)
// window_mode: WEBVIEW_WINDOW_MODE_OVERLAY for overlay popup, WEBVIEW_WINDOW_MODE_SEPARATE for separate window
WebViewOverlayWindow *webview_overlay_window_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlView *flutter_view,
    WebViewWindowMode window_mode,
    FlValue *initial_settings_map_or_null,
    WebKitWebContext *shared_context_or_null);

// Destroy overlay window
void webview_overlay_window_destroy(
    WebViewOverlayWindow *instance);

// Show overlay window
void webview_overlay_window_show(WebViewOverlayWindow *instance);

// Hide overlay window
void webview_overlay_window_hide(WebViewOverlayWindow *instance);

// Hide every embedded overlay except [keep]. Prevents a keep-alive / second
// WebKit surface from painting over the mailbox reader.
void webview_overlay_window_hide_others(
    GHashTable *overlay_windows,
    WebViewOverlayWindow *keep);

// Set overlay window position and size
void webview_overlay_window_set_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height);

// Set embedded overlay bounds from Flutter-view-local logical coordinates.
// view_* and device_pixel_ratio map logical FlView space onto GtkOverlay
// allocation (handles HiDPI / FlView inset inside the host overlay).
// sequence: optional monotonic id from Dart; values <= last applied are ignored.
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

// Set overlay bounds in absolute screen coordinates.
void webview_overlay_window_set_bounds_screen(
    WebViewOverlayWindow *instance,
    gint screen_x,
    gint screen_y,
    gint width,
    gint height);

// Get the WebKitGTK view instance
WebViewWebKitGTK *webview_overlay_window_get_webkit_view(
    WebViewOverlayWindow *instance);

// Move keyboard focus to the embedded WebKit editor (compose body).
void webview_overlay_window_grab_focus(WebViewOverlayWindow *instance);

// Return keyboard focus to Flutter's FlView (compose To / Cc / Subject fields).
// Required on Linux because GtkOverlay WebKit keeps GTK focus after grabFocus
// until something explicitly gives it back to FlView.
void webview_overlay_window_release_focus(WebViewOverlayWindow *instance);

// Position window next to main window (for separate window mode).
// Optional main_*: if main_width and main_height > 0, use (main_x, main_y, main_width, main_height)
// as the main window rect; otherwise query from parent window.
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
