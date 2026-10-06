#ifndef WEBVIEW_OVERLAY_EMBEDDED_H_
#define WEBVIEW_OVERLAY_EMBEDDED_H_

#include <gtk/gtk.h>
#include "webview_overlay_window.h"

G_BEGIN_DECLS

GtkWidget *webview_overlay_find_gtk_overlay_ancestor(GtkWidget *widget);

void webview_overlay_clear_source(guint *id);

void webview_overlay_cancel_idle_sources(WebViewOverlayWindow *instance);

void webview_overlay_raise_embedded_child(WebViewOverlayWindow *instance);

void webview_overlay_schedule_raise(WebViewOverlayWindow *instance);

void webview_overlay_on_embedded_container_realize(GtkWidget *widget,
                                                   gpointer user_data);

void webview_overlay_on_embedded_widget_size_allocate(GtkWidget *widget,
                                                      GtkAllocation *allocation,
                                                      gpointer user_data);

void webview_overlay_apply_embedded_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height,
    const gchar *log_prefix);

// Trailing-edge (per instance) onHostLayoutChanged so the final allocation of
// a maximize / drag-resize is always reported.
void webview_overlay_schedule_host_layout_changed(WebViewOverlayWindow *instance);

// Show the embedded container when Dart wants it visible and bounds allow it.
void webview_overlay_sync_embedded_visibility(WebViewOverlayWindow *instance);

void webview_overlay_on_host_size_allocate(GtkWidget *widget,
                                           GtkAllocation *allocation,
                                           gpointer user_data);

gboolean webview_overlay_on_get_child_position(GtkOverlay *overlay,
                                               GtkWidget *widget,
                                               GdkRectangle *allocation,
                                               gpointer user_data);

gboolean webview_overlay_on_parent_configure_event(GtkWidget *widget,
                                                   GdkEventConfigure *event,
                                                   gpointer user_data);

G_END_DECLS

#endif // WEBVIEW_OVERLAY_EMBEDDED_H_
