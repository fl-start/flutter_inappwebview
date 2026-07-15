#include "webview_plugin_lifecycle_handlers.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>

#include "webview_overlay_window.h"
#include "webview_webkitgtk.h"
#include "webview_webkitgtk_flutter_settings.h"
#include "webview_plugin_methods.h"

// Optional StandardCodec map under "settings" (same for create / ensure / open / applySettings).
static FlValue *webview_plugin_lookup_settings_map(FlValue *args)
{
  if (!args || fl_value_get_type(args) != FL_VALUE_TYPE_MAP)
  {
    return nullptr;
  }
  FlValue *s = fl_value_lookup_string(args, "settings");
  if (!s || fl_value_get_type(s) != FL_VALUE_TYPE_MAP)
  {
    return nullptr;
  }
  return s;
}

static FlView *get_flutter_view(FlPluginRegistrar *registrar)
{
  return fl_plugin_registrar_get_view(registrar);
}

bool webview_plugin_try_handle_lifecycle_method(
    FlMethodChannel *method_channel,
    FlPluginRegistrar *registrar,
    GHashTable *overlay_windows,
    const gchar *method,
    FlValue *args,
    FlMethodCall *method_call,
    FlMethodResponse **out_response)
{
  *out_response = nullptr;

  if (g_strcmp0(method, kMethodCreate) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *existing = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);
    if (existing)
    {
      // Hot restart can recreate a view with the same ID before old native
      // state is explicitly disposed; destroy stale overlay to prevent leaks
      // and "ghost" overlays surviving route/engine resets.
      webview_overlay_window_destroy(existing);
      g_hash_table_remove(overlay_windows, view_id_key);
    }

    WebViewWindowMode window_mode = WEBVIEW_WINDOW_MODE_OVERLAY;
    FlValue *window_mode_value = fl_value_lookup_string(args, "windowMode");

    if (window_mode_value && fl_value_get_type(window_mode_value) == FL_VALUE_TYPE_STRING)
    {
      const gchar *mode_str = fl_value_get_string(window_mode_value);

      if (g_strcmp0(mode_str, "separate") == 0)
        window_mode = WEBVIEW_WINDOW_MODE_SEPARATE;
    }

    FlView *flutter_view = get_flutter_view(registrar);
    if (!flutter_view)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UNAVAILABLE", "Flutter view not ready", nullptr));

      return true;
    }

    FlValue *settings_map = webview_plugin_lookup_settings_map(args);
    WebViewOverlayWindow *overlay_window = webview_overlay_window_new(
        method_channel, view_id, flutter_view, window_mode, settings_map);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INTERNAL_ERROR", "Failed to create overlay window", nullptr));

      return true;
    }

    g_hash_table_insert(overlay_windows, view_id_key, overlay_window);

    FlValue *user_scripts = fl_value_lookup_string(args, "userScripts");
    if (user_scripts && overlay_window->webkit_view)
    {
      webview_webkitgtk_add_user_scripts_from_flvalue(
          overlay_window->webkit_view, user_scripts);
    }

    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodEnsureEmailViewer) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *existing = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (existing)
    {
      FlValue *settings_map = webview_plugin_lookup_settings_map(args);
      if (settings_map)
      {
        webview_webkitgtk_apply_flutter_settings_map(existing->webkit_view, settings_map);
      }
      *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

      return true;
    }

    WebViewWindowMode window_mode = WEBVIEW_WINDOW_MODE_OVERLAY;
    FlValue *window_mode_value = fl_value_lookup_string(args, "windowMode");

    if (window_mode_value && fl_value_get_type(window_mode_value) == FL_VALUE_TYPE_STRING)
    {
      const gchar *mode_str = fl_value_get_string(window_mode_value);

      if (g_strcmp0(mode_str, "separate") == 0)
        window_mode = WEBVIEW_WINDOW_MODE_SEPARATE;
    }

    FlView *flutter_view = get_flutter_view(registrar);
    if (!flutter_view)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UNAVAILABLE", "Flutter view not ready", nullptr));

      return true;
    }

    FlValue *settings_map = webview_plugin_lookup_settings_map(args);
    WebViewOverlayWindow *overlay_window = webview_overlay_window_new(
        method_channel, view_id, flutter_view, window_mode, settings_map);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INTERNAL_ERROR", "Failed to create overlay window", nullptr));

      return true;
    }

    g_hash_table_insert(overlay_windows, view_id_key, overlay_window);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodOpenEmailViewer) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *url_value = fl_value_lookup_string(args, "url");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT ||
        url_value == nullptr || fl_value_get_type(url_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId/url are invalid", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *url = fl_value_get_string(url_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));

      return true;
    }

    gint width = 0, height = 0;
    FlValue *width_value = fl_value_lookup_string(args, "width");
    FlValue *height_value = fl_value_lookup_string(args, "height");

    if (width_value && fl_value_get_type(width_value) == FL_VALUE_TYPE_INT)
      width = fl_value_get_int(width_value);

    if (height_value && fl_value_get_type(height_value) == FL_VALUE_TYPE_INT)
      height = fl_value_get_int(height_value);

    webview_overlay_window_position_next_to_main(overlay_window, width, height, -1, -1, -1, -1);
    FlValue *settings_map_open = webview_plugin_lookup_settings_map(args);
    if (settings_map_open)
    {
      webview_webkitgtk_apply_flutter_settings_map(overlay_window->webkit_view, settings_map_open);
    }
    webview_webkitgtk_load_url(overlay_window->webkit_view, url);
    double zoom = 0.75;
    FlValue *zoom_value = fl_value_lookup_string(args, "zoom");

    if (zoom_value)
    {
      if (fl_value_get_type(zoom_value) == FL_VALUE_TYPE_FLOAT)
        zoom = fl_value_get_float(zoom_value);
      else if (fl_value_get_type(zoom_value) == FL_VALUE_TYPE_INT)
        zoom = (double)fl_value_get_int(zoom_value);
    }

    webview_webkitgtk_set_zoom_level(overlay_window->webkit_view, zoom);
    webview_overlay_window_show(overlay_window);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodDispose) == 0 || g_strcmp0(method, kMethodShow) == 0 ||
      g_strcmp0(method, kMethodHide) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));

      return true;
    }

    if (g_strcmp0(method, kMethodDispose) == 0)
    {
      gboolean keep_alive = FALSE;
      FlValue *keep_alive_value = fl_value_lookup_string(args, "keepAlive");
      if (keep_alive_value &&
          fl_value_get_type(keep_alive_value) == FL_VALUE_TYPE_BOOL)
      {
        keep_alive = fl_value_get_bool(keep_alive_value);
      }

      if (keep_alive)
      {
        webview_overlay_window_hide(overlay_window);
      }
      else
      {
        webview_overlay_window_destroy(overlay_window);
        g_hash_table_remove(overlay_windows, view_id_key);
      }
    }
    else if (g_strcmp0(method, kMethodShow) == 0)
    {
      webview_overlay_window_hide_others(overlay_windows, overlay_window);
      webview_overlay_window_show(overlay_window);
    }
    else
    {
      webview_overlay_window_hide(overlay_window);
    }

    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodGetMainWindowGeometry) == 0)
  {
    FlView *flutter_view = get_flutter_view(registrar);

    if (!flutter_view)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UNAVAILABLE", "Flutter view not ready", nullptr));

      return true;
    }

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(flutter_view));

    if (!toplevel || !GTK_IS_WINDOW(toplevel))
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UNAVAILABLE", "No toplevel window", nullptr));

      return true;
    }

    GtkWindow *win = GTK_WINDOW(toplevel);
    gint x = 0, y = 0, w = 800, h = 600;

    if (gtk_widget_get_realized(toplevel))
    {
      GdkWindow *gdk_win = gtk_widget_get_window(toplevel);
      if (gdk_win)
        gdk_window_get_origin(gdk_win, &x, &y);
    }

    gtk_window_get_size(win, &w, &h);

    if (x == 0 && y == 0)
      gtk_window_get_position(win, &x, &y);

    FlValue *result = fl_value_new_map();
    fl_value_set_string_take(result, "x", fl_value_new_int(x));
    fl_value_set_string_take(result, "y", fl_value_new_int(y));
    fl_value_set_string_take(result, "width", fl_value_new_int(w));
    fl_value_set_string_take(result, "height", fl_value_new_int(h));
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

    return true;
  }

  if (g_strcmp0(method, "grabFocus") == 0 ||
      g_strcmp0(method, "releaseFocus") == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));

      return true;
    }

    if (g_strcmp0(method, "releaseFocus") == 0)
      webview_overlay_window_release_focus(overlay_window);
    else
      webview_overlay_window_grab_focus(overlay_window);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodPositionNextToMain) == 0 || g_strcmp0(method, kMethodSetBounds) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));

      return true;
    }

    gint width = 0, height = 0;
    FlValue *v = fl_value_lookup_string(args, "width");

    if (v && fl_value_get_type(v) == FL_VALUE_TYPE_INT)
      width = fl_value_get_int(v);
    else if (v && fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT)
      width = (gint)lround(fl_value_get_float(v));

    v = fl_value_lookup_string(args, "height");

    if (v && fl_value_get_type(v) == FL_VALUE_TYPE_INT)
      height = fl_value_get_int(v);
    else if (v && fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT)
      height = (gint)lround(fl_value_get_float(v));

    if (g_strcmp0(method, kMethodSetBounds) == 0)
    {
      FlValue *x_value = fl_value_lookup_string(args, "x");
      FlValue *y_value = fl_value_lookup_string(args, "y");
      FlValue *screen_x_value = fl_value_lookup_string(args, "screenX");
      FlValue *screen_y_value = fl_value_lookup_string(args, "screenY");
      FlValue *view_w_value = fl_value_lookup_string(args, "viewWidth");
      FlValue *view_h_value = fl_value_lookup_string(args, "viewHeight");
      FlValue *dpr_value = fl_value_lookup_string(args, "devicePixelRatio");

      auto fl_as_double = [](FlValue *v) -> gdouble
      {
        if (!v)
          return 0.0;
        switch (fl_value_get_type(v))
        {
        case FL_VALUE_TYPE_FLOAT:
          return fl_value_get_float(v);
        case FL_VALUE_TYPE_INT:
          return (gdouble)fl_value_get_int(v);
        default:
          return 0.0;
        }
      };

      const gboolean has_xy =
          x_value && y_value &&
          (fl_value_get_type(x_value) == FL_VALUE_TYPE_INT ||
           fl_value_get_type(x_value) == FL_VALUE_TYPE_FLOAT) &&
          (fl_value_get_type(y_value) == FL_VALUE_TYPE_INT ||
           fl_value_get_type(y_value) == FL_VALUE_TYPE_FLOAT);

      if (has_xy)
      {
        // GtkOverlay embedded WebKit: map Flutter logical coords through the
        // FlView allocation onto GtkOverlay space (HiDPI / FlView inset safe).
        if (overlay_window->embedded_widget_mode)
        {
          const gdouble fx = fl_as_double(x_value);
          const gdouble fy = fl_as_double(y_value);
          const gdouble fw = width > 0 ? (gdouble)width : fl_as_double(fl_value_lookup_string(args, "width"));
          const gdouble fh = height > 0 ? (gdouble)height : fl_as_double(fl_value_lookup_string(args, "height"));
          const gdouble view_w = fl_as_double(view_w_value);
          const gdouble view_h = fl_as_double(view_h_value);
          gdouble dpr = fl_as_double(dpr_value);
          if (dpr <= 0.01)
            dpr = 1.0;

          // Prefer float width/height when Dart sends fractional logical sizes.
          gdouble use_w = fw;
          gdouble use_h = fh;
          FlValue *w_float = fl_value_lookup_string(args, "width");
          FlValue *h_float = fl_value_lookup_string(args, "height");
          if (w_float && fl_value_get_type(w_float) == FL_VALUE_TYPE_FLOAT)
            use_w = fl_value_get_float(w_float);
          if (h_float && fl_value_get_type(h_float) == FL_VALUE_TYPE_FLOAT)
            use_h = fl_value_get_float(h_float);

          webview_overlay_window_set_bounds_from_flutter(
              overlay_window,
              fx,
              fy,
              use_w > 0 ? use_w : 1.0,
              use_h > 0 ? use_h : 1.0,
              view_w,
              view_h,
              dpr);
        }
        else if (screen_x_value && screen_y_value &&
                 fl_value_get_type(screen_x_value) == FL_VALUE_TYPE_INT &&
                 fl_value_get_type(screen_y_value) == FL_VALUE_TYPE_INT)
        {
          webview_overlay_window_set_bounds_screen(
              overlay_window,
              fl_value_get_int(screen_x_value),
              fl_value_get_int(screen_y_value),
              width,
              height);
        }
        else
        {
          webview_overlay_window_set_bounds(
              overlay_window,
              (gint)lround(fl_as_double(x_value)),
              (gint)lround(fl_as_double(y_value)),
              width,
              height);
        }

        // One visible GtkOverlay WebKit at a time — keep-alive composer must
        // not stay painted over the mailbox reader (intermittent dual overlay).
        webview_overlay_window_hide_others(overlay_windows, overlay_window);
        webview_overlay_window_show(overlay_window);
      }
    }
    else
    {
      webview_overlay_window_position_next_to_main(overlay_window, width, height, -1, -1, -1, -1);
    }

    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodApplySettings) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *settings_map = webview_plugin_lookup_settings_map(args);

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    if (settings_map == nullptr)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "settings map is required", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);
    WebViewOverlayWindow *overlay_window = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));

      return true;
    }

    webview_webkitgtk_apply_flutter_settings_map(overlay_window->webkit_view, settings_map);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  return false;
}
