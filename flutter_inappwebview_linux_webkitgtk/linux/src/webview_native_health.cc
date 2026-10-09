#include "webview_native_health.h"
#include "webview_webkitgtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <webkit2/webkit2.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

static gchar g_last_error[256] = {0};

void webview_native_health_set_last_error(const gchar *reason)
{
  if (!reason || !reason[0])
  {
    g_last_error[0] = '\0';
    return;
  }
  g_strlcpy(g_last_error, reason, sizeof(g_last_error));
}

const gchar *webview_native_health_last_error(void)
{
  return g_last_error[0] ? g_last_error : nullptr;
}

static const gchar *gdk_backend_name(void)
{
  GdkDisplay *display = gdk_display_get_default();
  if (!display)
  {
    return "none";
  }
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY(display))
  {
    return "wayland";
  }
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY(display))
  {
    return "x11";
  }
#endif
  const gchar *name = gdk_display_get_name(display);
  return name ? name : "unknown";
}

static void set_string(FlValue *map, const gchar *key, const gchar *value)
{
  fl_value_set_string_take(map, key, fl_value_new_string(value ? value : ""));
}

void webview_native_health_fill_runtime(FlValue *map)
{
  if (!map || fl_value_get_type(map) != FL_VALUE_TYPE_MAP)
  {
    return;
  }

  gchar webkit_ver[32];
  g_snprintf(webkit_ver, sizeof(webkit_ver), "%u.%u.%u",
             webkit_get_major_version(),
             webkit_get_minor_version(),
             webkit_get_micro_version());
  gchar gtk_ver[32];
  g_snprintf(gtk_ver, sizeof(gtk_ver), "%u.%u.%u",
             gtk_get_major_version(),
             gtk_get_minor_version(),
             gtk_get_micro_version());
  gchar soup_ver[16];
  g_snprintf(soup_ver, sizeof(soup_ver), "%d", SOUP_MAJOR_VERSION);

  set_string(map, "gdkBackend", gdk_backend_name());
  set_string(map, "webkitVersion", webkit_ver);
  set_string(map, "gtkVersion", gtk_ver);
  set_string(map, "soupMajor", soup_ver);
  set_string(map, "webkitApi", "webkit2gtk-4.1");
  set_string(map, "webProcessSandbox", webview_webkitgtk_sandbox_status());

  const gchar *compositing = g_getenv("WEBKIT_DISABLE_COMPOSITING_MODE");
  set_string(map, "webkitDisableCompositing",
             compositing && compositing[0] ? compositing : "unset");

  if (g_last_error[0])
  {
    set_string(map, "lastError", g_last_error);
  }
}

FlValue *webview_native_health_from_overlay(WebViewOverlayWindow *instance)
{
  FlValue *map = fl_value_new_map();
  webview_native_health_fill_runtime(map);
  if (!instance)
  {
    fl_value_set_string_take(map, "loaded", fl_value_new_bool(FALSE));
    set_string(map, "embedding", "none");
    set_string(map, "reason", webview_native_health_last_error()
                                  ? webview_native_health_last_error()
                                  : "overlay_null");
    return map;
  }

  GtkWidget *web_view_widget =
      instance->webkit_view ? webview_webkitgtk_get_widget(instance->webkit_view)
                            : nullptr;
  const gboolean loaded = web_view_widget != nullptr;
  const gchar *embedding = "popup";
  if (instance->embedded_widget_mode)
  {
    embedding = "gtk_overlay";
  }
  else if (instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    embedding = "separate";
  }

  fl_value_set_string_take(map, "viewId", fl_value_new_int(instance->view_id));
  fl_value_set_string_take(map, "loaded", fl_value_new_bool(loaded));
  set_string(map, "embedding", embedding);
  fl_value_set_string_take(map, "hasGtkOverlayAncestor",
                           fl_value_new_bool(instance->embedding_overlay != nullptr));
  fl_value_set_string_take(map, "hasWebViewWidget", fl_value_new_bool(loaded));

  if (instance->flutter_view)
  {
    GtkWidget *fl = GTK_WIDGET(instance->flutter_view);
    GtkAllocation alloc;
    gtk_widget_get_allocation(fl, &alloc);
    fl_value_set_string_take(map, "flViewWidth", fl_value_new_int(alloc.width));
    fl_value_set_string_take(map, "flViewHeight", fl_value_new_int(alloc.height));
    fl_value_set_string_take(map, "flViewRealized",
                             fl_value_new_bool(gtk_widget_get_realized(fl)));
  }

  if (!loaded)
  {
    set_string(map, "reason", "webkit_widget_missing");
  }
  else if (!instance->embedded_widget_mode &&
           instance->window_mode == WEBVIEW_WINDOW_MODE_OVERLAY)
  {
    set_string(map, "warning", "missing_gtk_overlay_ancestor_popup_fallback");
  }

  return map;
}

FlValue *webview_native_health_snapshot(GHashTable *overlay_windows, gint64 view_id)
{
  WebViewOverlayWindow *overlay = nullptr;
  if (overlay_windows && view_id >= 0)
  {
    overlay = (WebViewOverlayWindow *)g_hash_table_lookup(
        overlay_windows, GSIZE_TO_POINTER((gsize)view_id));
  }
  else if (overlay_windows && g_hash_table_size(overlay_windows) == 1)
  {
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    g_hash_table_iter_init(&iter, overlay_windows);
    if (g_hash_table_iter_next(&iter, &key, &value))
    {
      overlay = (WebViewOverlayWindow *)value;
    }
  }

  FlValue *map = overlay ? webview_native_health_from_overlay(overlay)
                         : fl_value_new_map();
  if (!overlay)
  {
    webview_native_health_fill_runtime(map);
    fl_value_set_string_take(map, "loaded", fl_value_new_bool(FALSE));
    set_string(map, "embedding", "none");
    if (!webview_native_health_last_error())
    {
      set_string(map, "reason", "no_overlay");
    }
  }
  if (overlay_windows)
  {
    fl_value_set_string_take(
        map, "overlayCount",
        fl_value_new_int((gint64)g_hash_table_size(overlay_windows)));
  }
  return map;
}
