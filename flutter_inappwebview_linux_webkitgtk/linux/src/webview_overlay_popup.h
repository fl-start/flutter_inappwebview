#ifndef WEBVIEW_OVERLAY_POPUP_H_
#define WEBVIEW_OVERLAY_POPUP_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "webview_overlay_window.h"

G_BEGIN_DECLS

void webview_overlay_apply_screen_bounds(
    WebViewOverlayWindow *instance,
    gint screen_x,
    gint screen_y,
    gint width,
    gint height,
    const gchar *log_prefix);

gboolean webview_overlay_on_window_delete_event(GtkWidget *widget,
                                                GdkEvent *event,
                                                gpointer user_data);

gboolean webview_overlay_on_window_key_press_event(GtkWidget *widget,
                                                   GdkEventKey *event,
                                                   gpointer user_data);

G_END_DECLS

#endif // WEBVIEW_OVERLAY_POPUP_H_
