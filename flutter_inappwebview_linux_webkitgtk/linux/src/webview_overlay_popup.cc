#include "webview_overlay_popup.h"

#include "webview_overlay_debug.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

void webview_overlay_apply_screen_bounds(
    WebViewOverlayWindow *instance,
    gint screen_x,
    gint screen_y,
    gint width,
    gint height,
    const gchar *log_prefix)
{
  if (!instance || !instance->window)
    return;

  if (instance->has_applied_screen_bounds &&
      instance->last_screen_x == screen_x &&
      instance->last_screen_y == screen_y &&
      instance->last_screen_width == width &&
      instance->last_screen_height == height)
  {
    return;
  }

  instance->has_applied_screen_bounds = TRUE;
  instance->last_screen_x = screen_x;
  instance->last_screen_y = screen_y;
  instance->last_screen_width = width;
  instance->last_screen_height = height;

  gtk_window_move(instance->window, screen_x, screen_y);
  gtk_window_resize(instance->window, width, height);

  if (instance->container)
  {
    gtk_widget_set_size_request(instance->container, width, height);
    gtk_widget_queue_resize(instance->container);
  }

  if (instance->webkit_view)
  {
    GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);
    if (web_view_widget)
    {
      gtk_widget_set_size_request(web_view_widget, width, height);
      gtk_widget_queue_resize(web_view_widget);
    }
  }

  gtk_widget_queue_resize(GTK_WIDGET(instance->window));

  g_print("🐧 %s: %dx%d @ screen(%d,%d) (view_id: %ld)\n",
          log_prefix, width, height, screen_x, screen_y, instance->view_id);
}

static const gchar *key_label_from_gdk(guint keyval)
{
  switch (keyval)
  {
  case GDK_KEY_Up:
    return "Arrow Up";
  case GDK_KEY_Down:
    return "Arrow Down";
  case GDK_KEY_Left:
    return "Arrow Left";
  case GDK_KEY_Right:
    return "Arrow Right";
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
    return "Enter";
  case GDK_KEY_Escape:
    return "Escape";
  case GDK_KEY_Delete:
    return "Delete";
  case GDK_KEY_Home:
    return "Home";
  case GDK_KEY_End:
    return "End";
  case GDK_KEY_Page_Up:
    return "Page Up";
  case GDK_KEY_Page_Down:
    return "Page Down";
  case GDK_KEY_F5:
    return "F5";
  case GDK_KEY_slash:
    return "/";
  default:
    break;
  }

  gunichar uni = gdk_keyval_to_unicode(keyval);
  if (uni != 0)
  {
    static gchar buf[8];
    const gunichar upper = g_unichar_toupper(uni);
    const gint len = g_unichar_to_utf8(upper, buf);
    if (len > 0)
    {
      buf[len] = '\0';
      return buf;
    }
  }

  return nullptr;
}

static void emit_raw_key_event(
    WebViewOverlayWindow *instance,
    const gchar *key,
    gboolean ctrl,
    gboolean shift,
    gboolean alt,
    gboolean meta)
{
  if (!instance || !instance->method_channel || !key)
    return;
  g_autoptr(FlValue) map = fl_value_new_map();
  fl_value_set_string_take(map, "key", fl_value_new_string(key));
  fl_value_set_string_take(map, "ctrl", fl_value_new_bool(ctrl));
  fl_value_set_string_take(map, "shift", fl_value_new_bool(shift));
  fl_value_set_string_take(map, "alt", fl_value_new_bool(alt));
  fl_value_set_string_take(map, "meta", fl_value_new_bool(meta));
  fl_method_channel_invoke_method(
      instance->method_channel, "onRawKeyEvent", map, nullptr, nullptr, nullptr);
}

gboolean webview_overlay_on_window_delete_event(GtkWidget *widget,
                                                GdkEvent *event,
                                                gpointer user_data)
{
  (void)event;
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  gtk_widget_hide(widget);

  if (instance && instance->method_channel &&
      instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    g_autoptr(FlValue) map = fl_value_new_map();
    fl_value_set_string_take(map, "closed", fl_value_new_string("true"));
    fl_method_channel_invoke_method(
        instance->method_channel, "onWindowClosed", map, nullptr, nullptr, nullptr);
    g_print("🐧 Notified Dart about window close\n");
  }

  return TRUE;
}

gboolean webview_overlay_on_window_key_press_event(GtkWidget *widget,
                                                   GdkEventKey *event,
                                                   gpointer user_data)
{
  (void)widget;
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance || !event)
    return FALSE;

  const gboolean ctrl = (event->state & GDK_CONTROL_MASK) != 0;
  const gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;
  const gboolean alt = (event->state & GDK_MOD1_MASK) != 0;
  const gboolean meta = (event->state & GDK_SUPER_MASK) != 0;
  const gchar *key = key_label_from_gdk(event->keyval);
  if (!key)
    return FALSE;

  emit_raw_key_event(instance, key, ctrl, shift, alt, meta);

  // Shortcuts still reach Dart, but a plain printable key must also reach
  // WebKit or nothing can be typed into the page. Named keys and
  // Ctrl/Alt/Super chords stay consumed as shortcuts.
  const gboolean printable = gdk_keyval_to_unicode(event->keyval) != 0 &&
                             !g_unichar_iscntrl(gdk_keyval_to_unicode(event->keyval));
  return !(printable && !ctrl && !alt && !meta);
}

void webview_overlay_window_position_next_to_main(
    WebViewOverlayWindow *instance,
    gint width,
    gint height,
    gint main_x,
    gint main_y,
    gint main_width,
    gint main_height)
{
  if (!instance || !instance->window)
    return;

  if (instance->window_mode != WEBVIEW_WINDOW_MODE_SEPARATE)
    return;

  GdkDisplay *display = gdk_display_get_default();
#ifdef GDK_WINDOWING_WAYLAND
  if (display && GDK_IS_WAYLAND_DISPLAY(display))
  {
    gint parent_w = 800, parent_h = 600;
    gtk_window_get_size(instance->parent_window, &parent_w, &parent_h);
    gint webview_width = width > 0 ? width : 800;
    gint webview_height = height > 0 ? height : parent_h;
    if (webview_height < 420)
      webview_height = 420;

    gtk_window_set_position(instance->window, GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_resize(instance->window, webview_width, webview_height);
    instance->width = webview_width;
    instance->height = webview_height;
    g_print("🐧 Wayland: centered separate window %dx%d (compositor-managed position)\n",
            webview_width, webview_height);
    return;
  }
#endif

  gint parent_x = 0, parent_y = 0;
  gint parent_width = 800, parent_height = 600;

  if (main_width > 0 && main_height > 0)
  {
    parent_x = main_x;
    parent_y = main_y;
    parent_width = main_width;
    parent_height = main_height;
    g_print("🐧 Using main window rect from Dart: %dx%d @ (%d,%d)\n",
            parent_width, parent_height, parent_x, parent_y);
  }
  else
  {
    if (instance->flutter_view)
    {
      GtkWidget *flutter_widget = GTK_WIDGET(instance->flutter_view);
      if (gtk_widget_get_realized(flutter_widget))
      {
        GdkWindow *flutter_gdk = gtk_widget_get_window(flutter_widget);
        if (flutter_gdk)
        {
          gdk_window_get_origin(flutter_gdk, &parent_x, &parent_y);
        }
      }
      const gint alloc_w = gtk_widget_get_allocated_width(flutter_widget);
      const gint alloc_h = gtk_widget_get_allocated_height(flutter_widget);
      if (alloc_w > 0 && alloc_h > 0)
      {
        parent_width = alloc_w;
        parent_height = alloc_h;
      }
    }

    GtkWidget *parent_widget = GTK_WIDGET(instance->parent_window);
    if (gtk_widget_get_realized(parent_widget))
    {
      GdkWindow *parent_gdk = gtk_widget_get_window(parent_widget);
      if (parent_gdk)
      {
        if (parent_x == 0 && parent_y == 0)
        {
          gdk_window_get_origin(parent_gdk, &parent_x, &parent_y);
        }
      }
    }
    if (parent_width <= 0 || parent_height <= 0)
    {
      gtk_window_get_size(instance->parent_window, &parent_width, &parent_height);
    }
    if (parent_x == 0 && parent_y == 0)
    {
      gtk_window_get_position(instance->parent_window, &parent_x, &parent_y);
    }
  }

  const gint kViewerGapPx = 24;
  const gint kViewerOffsetY = 28;
  const gint kScreenMarginPx = 12;

  gint webview_width = width > 0 ? width : 800;
  gint webview_height = parent_height;
  if (webview_height < 420)
    webview_height = 420;

  GdkRectangle monitor_geom = {0, 0, 1920, 1080};
  display = gtk_widget_get_display(GTK_WIDGET(instance->window));
  if (display)
  {
    GdkMonitor *monitor = gdk_display_get_monitor_at_point(display, parent_x, parent_y);
    if (!monitor)
      monitor = gdk_display_get_primary_monitor(display);
    if (monitor)
      gdk_monitor_get_geometry(monitor, &monitor_geom);
  }

  const gint monitor_left = monitor_geom.x + kScreenMarginPx;
  const gint monitor_top = monitor_geom.y + kScreenMarginPx;
  const gint monitor_right = monitor_geom.x + monitor_geom.width - kScreenMarginPx;
  const gint monitor_bottom = monitor_geom.y + monitor_geom.height - kScreenMarginPx;

  if (webview_height > (monitor_bottom - monitor_top))
    webview_height = monitor_bottom - monitor_top;

  const gint right_x = parent_x + parent_width + kViewerGapPx;
  const gint left_x = parent_x - webview_width - kViewerGapPx;
  const gboolean fits_right = (right_x + webview_width) <= monitor_right;
  const gboolean fits_left = left_x >= monitor_left;

  gint viewer_x = right_x;
  if (fits_right)
    viewer_x = right_x;
  else if (fits_left)
    viewer_x = left_x;
  else
  {
    if (viewer_x + webview_width > monitor_right)
      viewer_x = monitor_right - webview_width;
    if (viewer_x < monitor_left)
      viewer_x = monitor_left;
  }

  gint viewer_y = parent_y + kViewerOffsetY;
  if (viewer_y + webview_height > monitor_bottom)
    viewer_y = monitor_bottom - webview_height;
  if (viewer_y < monitor_top)
    viewer_y = monitor_top;

  gtk_window_resize(instance->window, webview_width, webview_height);
  gtk_window_move(instance->window, viewer_x, viewer_y);

  instance->width = webview_width;
  instance->height = webview_height;

  g_print("🐧 Positioned separate window: %dx%d @ (%d,%d) next to main (parent %dx%d @ (%d,%d), monitor %dx%d @ (%d,%d))\n",
          webview_width, webview_height, viewer_x, viewer_y,
          parent_width, parent_height, parent_x, parent_y,
          monitor_geom.width, monitor_geom.height, monitor_geom.x, monitor_geom.y);
}
