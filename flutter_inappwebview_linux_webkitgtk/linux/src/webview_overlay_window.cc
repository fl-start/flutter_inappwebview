#include "webview_overlay_window.h"

#include "webview_overlay_coords.h"
#include "webview_overlay_debug.h"
#include "webview_overlay_embedded.h"
#include "webview_overlay_popup.h"
#include "webview_native_health.h"
#include "webview_webkitgtk.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>

static WebViewOverlayWindow *fail_overlay_new(WebViewOverlayWindow *instance,
                                              const gchar *reason)
{
  webview_native_health_set_last_error(reason);
  g_warning("Scomm WebKitGTK overlay create failed: %s", reason);
  g_free(instance);
  return nullptr;
}

WebViewOverlayWindow *webview_overlay_window_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlView *flutter_view,
    WebViewWindowMode window_mode,
    FlValue *initial_settings_map_or_null,
    WebKitWebContext *shared_context_or_null)
{
  WebViewOverlayWindow *instance = g_new0(WebViewOverlayWindow, 1);
  instance->method_channel = method_channel;
  instance->view_id = view_id;
  instance->x = 0;
  instance->y = 0;
  instance->width = 800;
  instance->height = 600;
  instance->flutter_view = flutter_view;
  instance->window_mode = window_mode;
  instance->embedded_widget_mode = FALSE;
  instance->embedding_overlay = nullptr;

  if (!flutter_view)
    return fail_overlay_new(instance, "flutter_view_null");

  GtkWidget *flutter_widget = GTK_WIDGET(flutter_view);
  if (!flutter_widget)
    return fail_overlay_new(instance, "flutter_widget_null");

  GtkWidget *toplevel = gtk_widget_get_toplevel(flutter_widget);
  if (!toplevel || !GTK_IS_WINDOW(toplevel))
    return fail_overlay_new(instance, "toplevel_window_missing");
  instance->parent_window = GTK_WINDOW(toplevel);

  GtkWidget *flutter_parent = gtk_widget_get_parent(flutter_widget);
  GtkWidget *overlay_host = webview_overlay_find_gtk_overlay_ancestor(flutter_widget);
  if (instance->window_mode == WEBVIEW_WINDOW_MODE_OVERLAY && overlay_host)
  {
    instance->embedded_widget_mode = TRUE;
    instance->embedding_overlay = overlay_host;
    if (flutter_parent != overlay_host)
    {
      g_print(
          "🐧 FlView parent is %s; embedding into Overlay ancestor\n",
          flutter_parent ? G_OBJECT_TYPE_NAME(flutter_parent) : "(null)");
    }
    instance->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(instance->container, GTK_ALIGN_START);
    gtk_widget_set_valign(instance->container, GTK_ALIGN_START);
    gtk_widget_set_hexpand(instance->container, FALSE);
    gtk_widget_set_vexpand(instance->container, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(instance->embedding_overlay), instance->container);
    gtk_overlay_set_overlay_pass_through(
        GTK_OVERLAY(instance->embedding_overlay), instance->container, FALSE);
    gtk_widget_hide(instance->container);
    g_signal_connect(
        instance->container,
        "realize",
        G_CALLBACK(webview_overlay_on_embedded_container_realize),
        instance);

    instance->child_position_handler_id = g_signal_connect(
        G_OBJECT(instance->embedding_overlay), "get-child-position",
        G_CALLBACK(webview_overlay_on_get_child_position), instance);
    instance->host_layout_flutter_handler_id = g_signal_connect(
        flutter_widget, "size-allocate",
        G_CALLBACK(webview_overlay_on_host_size_allocate), instance);
    instance->host_layout_overlay_handler_id = g_signal_connect(
        instance->embedding_overlay, "size-allocate",
        G_CALLBACK(webview_overlay_on_host_size_allocate), instance);
    // Wayland often skips configure-event; toplevel size-allocate still fires
    // on maximize / drag-resize.
    if (instance->parent_window)
    {
      instance->host_layout_toplevel_handler_id = g_signal_connect(
          GTK_WIDGET(instance->parent_window), "size-allocate",
          G_CALLBACK(webview_overlay_on_host_size_allocate), instance);
    }
    webview_native_health_set_last_error(nullptr);
  }

  if (!instance->embedded_widget_mode &&
      instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    instance->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(instance->window, "Email Viewer");

    gint parent_width = 800, parent_height = 800;
    if (instance->parent_window)
      gtk_window_get_size(instance->parent_window, &parent_width, &parent_height);

    gtk_window_set_default_size(instance->window, 800, parent_height);
    gtk_window_set_resizable(instance->window, TRUE);
    gtk_window_set_decorated(instance->window, TRUE);
    gtk_window_set_deletable(instance->window, TRUE);
    gtk_window_set_keep_above(instance->window, TRUE);
    gtk_window_set_transient_for(instance->window, instance->parent_window);
    g_print("🐧 Separate viewer mode active (X11)\n");
    gtk_window_set_skip_taskbar_hint(instance->window, FALSE);
    gtk_window_set_skip_pager_hint(instance->window, FALSE);
  }
  else if (!instance->embedded_widget_mode)
  {
    instance->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
    gtk_window_set_decorated(instance->window, FALSE);
    gtk_window_set_resizable(instance->window, FALSE);
    gtk_window_set_skip_taskbar_hint(instance->window, TRUE);
    gtk_window_set_skip_pager_hint(instance->window, TRUE);
    gtk_window_set_transient_for(instance->window, instance->parent_window);
    gtk_window_set_modal(instance->window, FALSE);
    gtk_window_set_type_hint(instance->window, GDK_WINDOW_TYPE_HINT_UTILITY);
    webview_native_health_set_last_error(
        "missing_gtk_overlay_ancestor_popup_fallback");
    g_warning(
        "Scomm WebKitGTK: no GtkOverlay ancestor for FlView; using a popup "
        "window. The mailbox WebView may appear blank or misaligned "
        "(view_id=%ld).",
        (long)view_id);
  }

  instance->parent_configure_handler_id = g_signal_connect(
      G_OBJECT(instance->parent_window), "configure-event",
      G_CALLBACK(webview_overlay_on_parent_configure_event), instance);

  if (!instance->embedded_widget_mode &&
      instance->window_mode == WEBVIEW_WINDOW_MODE_OVERLAY)
  {
    gtk_window_set_accept_focus(instance->window, FALSE);
    gtk_window_set_focus_on_map(instance->window, FALSE);
  }
  else if (!instance->embedded_widget_mode)
  {
    gtk_window_set_accept_focus(instance->window, TRUE);
    gtk_window_set_focus_on_map(instance->window, FALSE);
  }

  if (!instance->embedded_widget_mode)
  {
    instance->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(instance->window), instance->container);
  }

  instance->webkit_view =
      webview_webkitgtk_new(method_channel, view_id, initial_settings_map_or_null,
                            shared_context_or_null);
  GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);

  if (web_view_widget)
  {
    gtk_box_pack_start(GTK_BOX(instance->container), web_view_widget, TRUE, TRUE, 0);
  }
  else
  {
    webview_native_health_set_last_error("webkit_widget_missing");
    g_warning(
        "Scomm WebKitGTK: WebKitWebView widget was not created (view_id=%ld).",
        (long)view_id);
    webview_overlay_window_destroy(instance);
    return nullptr;
  }

  if (!instance->embedded_widget_mode)
  {
    g_signal_connect(instance->window, "delete-event",
                     G_CALLBACK(webview_overlay_on_window_delete_event), instance);
    g_signal_connect(instance->window, "key-press-event",
                     G_CALLBACK(webview_overlay_on_window_key_press_event), instance);
    gtk_window_set_default_size(instance->window, instance->width, instance->height);
    gtk_window_move(instance->window, instance->x, instance->y);
    gtk_widget_hide(GTK_WIDGET(instance->window));
    g_print("🐧 Overlay window created (popup mode, view_id: %ld)\n", view_id);
  }
  else
  {
    g_print("🐧 Overlay window created (embedded mode, view_id: %ld)\n", view_id);
  }

  {
    g_autoptr(FlValue) health = webview_native_health_from_overlay(instance);
    FlValue *emb = fl_value_lookup_string(health, "embedding");
    FlValue *gdk = fl_value_lookup_string(health, "gdkBackend");
    FlValue *ver = fl_value_lookup_string(health, "webkitVersion");
    g_message(
        "Scomm WebKitGTK ready: view_id=%ld embedding=%s gdk=%s webkit=%s",
        (long)view_id,
        emb && fl_value_get_type(emb) == FL_VALUE_TYPE_STRING
            ? fl_value_get_string(emb)
            : "?",
        gdk && fl_value_get_type(gdk) == FL_VALUE_TYPE_STRING
            ? fl_value_get_string(gdk)
            : "?",
        ver && fl_value_get_type(ver) == FL_VALUE_TYPE_STRING
            ? fl_value_get_string(ver)
            : "?");
  }

  return instance;
}

void webview_overlay_window_destroy(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;

  g_print("🐧 Destroying overlay window (view_id: %ld)\n", instance->view_id);

  webview_overlay_cancel_idle_sources(instance);

  if (instance->parent_configure_handler_id > 0 && instance->parent_window)
  {
    g_signal_handler_disconnect(G_OBJECT(instance->parent_window),
                                instance->parent_configure_handler_id);
    instance->parent_configure_handler_id = 0;
  }

  if (instance->child_position_handler_id > 0 &&
      instance->embedding_overlay &&
      GTK_IS_WIDGET(instance->embedding_overlay))
  {
    g_signal_handler_disconnect(G_OBJECT(instance->embedding_overlay),
                                instance->child_position_handler_id);
    instance->child_position_handler_id = 0;
  }

  if (instance->host_layout_overlay_handler_id > 0 &&
      instance->embedding_overlay &&
      GTK_IS_WIDGET(instance->embedding_overlay))
  {
    g_signal_handler_disconnect(G_OBJECT(instance->embedding_overlay),
                                instance->host_layout_overlay_handler_id);
    instance->host_layout_overlay_handler_id = 0;
  }

  if (instance->host_layout_flutter_handler_id > 0 && instance->flutter_view)
  {
    g_signal_handler_disconnect(GTK_WIDGET(instance->flutter_view),
                                instance->host_layout_flutter_handler_id);
    instance->host_layout_flutter_handler_id = 0;
  }

  if (instance->host_layout_toplevel_handler_id > 0 && instance->parent_window &&
      GTK_IS_WIDGET(instance->parent_window))
  {
    g_signal_handler_disconnect(G_OBJECT(instance->parent_window),
                                instance->host_layout_toplevel_handler_id);
    instance->host_layout_toplevel_handler_id = 0;
  }

  if (instance->webkit_view)
  {
    webview_webkitgtk_destroy(instance->webkit_view);
    instance->webkit_view = nullptr;
  }

  if (instance->window)
  {
    GtkWidget *window_widget = GTK_WIDGET(instance->window);
    if (GTK_IS_WIDGET(window_widget))
      gtk_widget_destroy(window_widget);
    instance->window = nullptr;
  }
  else if (instance->container)
  {
    GtkWidget *parent = gtk_widget_get_parent(instance->container);
    if (parent && GTK_IS_OVERLAY(parent))
    {
      if (GTK_IS_WIDGET(instance->container))
        gtk_container_remove(GTK_CONTAINER(parent), instance->container);
    }
    else if (GTK_IS_WIDGET(instance->container))
    {
      gtk_widget_destroy(instance->container);
    }
    instance->container = nullptr;
  }

  g_free(instance);
}

void webview_overlay_window_show(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;

  if (instance->embedded_widget_mode)
  {
    instance->wants_visible = TRUE;
    webview_overlay_sync_embedded_visibility(instance);
    if (!instance->logged_visible)
    {
      instance->logged_visible = TRUE;
      g_message("Scomm WebKitGTK overlay show: view_id=%ld embedding=gtk_overlay "
                "bounds_applied=%d",
                (long)instance->view_id,
                instance->has_applied_overlay_bounds ? 1 : 0);
    }
    return;
  }

  if (!instance->window)
    return;

  if (instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    webview_overlay_window_position_next_to_main(
        instance, instance->width, instance->height, -1, -1, -1, -1);
    gtk_window_set_keep_above(instance->window, TRUE);
    gtk_window_set_position(instance->window, GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_type_hint(instance->window, GDK_WINDOW_TYPE_HINT_DIALOG);
  }

  if (!gtk_widget_get_visible(GTK_WIDGET(instance->window)))
  {
    gtk_widget_show_all(GTK_WIDGET(instance->window));
    gtk_window_present(instance->window);
  }

  if (!instance->logged_visible)
  {
    instance->logged_visible = TRUE;
    g_message("Scomm WebKitGTK overlay show: view_id=%ld embedding=%s",
              (long)instance->view_id,
              instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE ? "separate"
                                                                   : "popup");
  }
}

void webview_overlay_window_hide(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;
  if (instance->embedded_widget_mode)
  {
    instance->wants_visible = FALSE;
    if (instance->container)
      gtk_widget_hide(instance->container);
  }
  else if (instance->window)
  {
    gtk_widget_hide(GTK_WIDGET(instance->window));
  }
  if (instance->logged_visible)
  {
    instance->logged_visible = FALSE;
    g_message(
        "Scomm WebKitGTK overlay hide: view_id=%ld embedding=%s",
        (long)instance->view_id,
        instance->embedded_widget_mode
            ? "gtk_overlay"
            : (instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE ? "separate"
                                                                    : "popup"));
  }
}

typedef struct
{
  WebViewOverlayWindow *keep;
} HideOthersData;

static void hide_other_overlay_cb(gpointer key, gpointer value, gpointer user_data)
{
  (void)key;
  HideOthersData *data = static_cast<HideOthersData *>(user_data);
  WebViewOverlayWindow *other = static_cast<WebViewOverlayWindow *>(value);
  if (!other || other == data->keep)
    return;
  if (!other->embedded_widget_mode)
    return;
  if (other->container && gtk_widget_get_visible(other->container))
  {
    coord_print(
        "🐧 Hiding sibling overlay view_id=%ld (keeping %ld)\n",
        other->view_id,
        data->keep ? data->keep->view_id : -1);
    webview_overlay_window_hide(other);
  }
}

void webview_overlay_window_hide_others(
    GHashTable *overlay_windows,
    WebViewOverlayWindow *keep)
{
  if (!overlay_windows || !keep)
    return;
  HideOthersData data = {.keep = keep};
  g_hash_table_foreach(overlay_windows, hide_other_overlay_cb, &data);
}

void webview_overlay_window_set_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height)
{
  if (!instance)
    return;

  instance->x = x;
  instance->y = y;
  instance->width = width;
  instance->height = height;

  if (instance->embedded_widget_mode)
  {
    webview_overlay_apply_embedded_bounds(
        instance, x, y, width, height, "Overlay bounds (embedded)");
    return;
  }

  if (!instance->window)
    return;

  if (instance->flutter_view)
  {
    GtkWidget *flutter_widget = GTK_WIDGET(instance->flutter_view);
    if (gtk_widget_get_realized(flutter_widget))
    {
      GdkWindow *gdk_window = gtk_widget_get_window(flutter_widget);
      if (gdk_window)
      {
        gint client_x = 0, client_y = 0;
        gdk_window_get_origin(gdk_window, &client_x, &client_y);
        webview_overlay_apply_screen_bounds(
            instance,
            client_x + x,
            client_y + y,
            width,
            height,
            "Overlay bounds");
        return;
      }
    }
  }

  GtkWindow *parent = instance->parent_window;
  if (parent)
  {
    gint parent_x = 0, parent_y = 0;
    gtk_window_get_position(parent, &parent_x, &parent_y);
    webview_overlay_apply_screen_bounds(
        instance,
        parent_x + x,
        parent_y + y,
        width,
        height,
        "Overlay bounds (fallback)");
  }
  else
  {
    webview_overlay_apply_screen_bounds(
        instance, x, y, width, height, "Overlay bounds (no parent)");
  }
}

void webview_overlay_window_reset_bounds_sequence(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;
  instance->last_bounds_sequence = 0;
}

gboolean webview_overlay_window_set_bounds_from_flutter(
    WebViewOverlayWindow *instance,
    gdouble x,
    gdouble y,
    gdouble width,
    gdouble height,
    gdouble view_width,
    gdouble view_height,
    gdouble device_pixel_ratio,
    gint64 sequence,
    gint64 generation)
{
  if (!instance)
    return FALSE;

  if (generation > 0 && generation != instance->last_generation)
  {
    instance->last_generation = generation;
    instance->last_bounds_sequence = 0;
  }

  if (sequence > 0 && sequence <= instance->last_bounds_sequence)
  {
    const gint64 now_us = g_get_monotonic_time();
    if (now_us - instance->last_stale_sequence_log_us > G_USEC_PER_SEC)
    {
      instance->last_stale_sequence_log_us = now_us;
      g_message(
          "Scomm WebKitGTK setBounds rejected: view_id=%ld seq=%" G_GINT64_FORMAT
          " <= last=%" G_GINT64_FORMAT,
          (long)instance->view_id,
          sequence,
          instance->last_bounds_sequence);
    }
    return FALSE;
  }
  if (sequence > 0)
    instance->last_bounds_sequence = sequence;

  gint overlay_x = 0;
  gint overlay_y = 0;
  gint overlay_w = 1;
  gint overlay_h = 1;
  gboolean stale = FALSE;
  webview_overlay_convert_flutter_bounds(
      instance,
      x,
      y,
      width,
      height,
      view_width,
      view_height,
      &overlay_x,
      &overlay_y,
      &overlay_w,
      &overlay_h,
      &stale);

  const gboolean bounds_changed =
      overlay_x != instance->x || overlay_y != instance->y ||
      overlay_w != instance->width || overlay_h != instance->height ||
      !instance->has_applied_overlay_bounds;

  instance->x = overlay_x;
  instance->y = overlay_y;
  instance->width = overlay_w;
  instance->height = overlay_h;

  if (bounds_changed)
  {
    g_message(
        "Scomm WebKitGTK setBounds: view_id=%ld flutter=%.0f,%.0f %.0fx%.0f "
        "viewLogical=%.0fx%.0f dpr=%.2f -> overlay=%d,%d %dx%d%s",
        (long)instance->view_id,
        x,
        y,
        width,
        height,
        view_width,
        view_height,
        device_pixel_ratio,
        overlay_x,
        overlay_y,
        overlay_w,
        overlay_h,
        stale ? " (stale view metrics)" : "");
  }

  // Applying identity coordinates is the best available guess while Dart is
  // behind the window metrics; ask Dart to re-measure once it catches up.
  if (stale)
    webview_overlay_schedule_host_layout_changed(instance);

  if (instance->embedded_widget_mode)
  {
    webview_overlay_apply_embedded_bounds(
        instance,
        overlay_x,
        overlay_y,
        overlay_w,
        overlay_h,
        "Overlay bounds (flutter->overlay)");
    return TRUE;
  }

  webview_overlay_window_set_bounds(
      instance, overlay_x, overlay_y, overlay_w, overlay_h);
  return TRUE;
}

void webview_overlay_window_set_bounds_screen(
    WebViewOverlayWindow *instance,
    gint screen_x,
    gint screen_y,
    gint width,
    gint height)
{
  if (!instance)
    return;

  if (instance->embedded_widget_mode)
  {
    if (instance->flutter_view)
    {
      GtkWidget *flutter_widget = GTK_WIDGET(instance->flutter_view);
      if (gtk_widget_get_realized(flutter_widget))
      {
        GdkWindow *gdk_window = gtk_widget_get_window(flutter_widget);
        if (gdk_window)
        {
          gint client_x = 0, client_y = 0;
          gdk_window_get_origin(gdk_window, &client_x, &client_y);
          webview_overlay_apply_embedded_bounds(
              instance,
              screen_x - client_x,
              screen_y - client_y,
              width,
              height,
              "Overlay bounds (embedded screen->local)");
          instance->x = screen_x - client_x;
          instance->y = screen_y - client_y;
          instance->width = width;
          instance->height = height;
          return;
        }
      }
    }

    g_print(
        "⚠️ Overlay bounds (embedded): skipped update, local origin unavailable "
        "(screen %d,%d %dx%d, view_id: %ld)\n",
        screen_x,
        screen_y,
        width,
        height,
        instance->view_id);
    return;
  }

  instance->x = screen_x;
  instance->y = screen_y;
  instance->width = width;
  instance->height = height;

  if (!instance->window)
    return;

  webview_overlay_apply_screen_bounds(
      instance,
      screen_x,
      screen_y,
      width,
      height,
      "Overlay bounds (screen-absolute)");
}

WebViewWebKitGTK *webview_overlay_window_get_webkit_view(
    WebViewOverlayWindow *instance)
{
  if (!instance)
    return nullptr;
  return instance->webkit_view;
}

void webview_overlay_window_grab_focus(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->webkit_view)
    return;

  GtkWidget *web_view_widget =
      webview_webkitgtk_get_widget(instance->webkit_view);
  if (!web_view_widget)
    return;

  if (!gtk_widget_get_realized(web_view_widget))
    gtk_widget_realize(web_view_widget);

  gtk_widget_grab_focus(web_view_widget);
}

void webview_overlay_window_release_focus(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->flutter_view)
    return;

  GtkWidget *flutter_widget = GTK_WIDGET(instance->flutter_view);
  if (!flutter_widget)
    return;

  if (!gtk_widget_get_realized(flutter_widget))
    gtk_widget_realize(flutter_widget);

  gtk_widget_grab_focus(flutter_widget);
}
