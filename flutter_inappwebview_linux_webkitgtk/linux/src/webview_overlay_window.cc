#include "webview_overlay_window.h"
#include "webview_webkitgtk.h"
#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define g_print(...) ((void)0)
#endif

// Include Wayland-specific headers for display type checking
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

// Forward declaration for window close handler
static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);

// Handler for parent window configure event (position/size changes)
static gboolean on_parent_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data);
static gboolean on_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

static void apply_embedded_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height,
    const gchar *log_prefix)
{
  if (!instance || !instance->embedded_widget_mode || !instance->container)
    return;

  // Clamp embedded bounds to the host overlay allocation so native WebKit
  // never overflows the Flutter conversation pane (e.g. when Sentria side panel
  // opens and available width shrinks).
  gint bounded_x = x;
  gint bounded_y = y;
  gint bounded_width = width;
  gint bounded_height = height;

  GtkWidget *host = instance->embedding_overlay;
  if (host && gtk_widget_get_realized(host))
  {
    const gint host_w = gtk_widget_get_allocated_width(host);
    const gint host_h = gtk_widget_get_allocated_height(host);
    if (host_w > 0 && host_h > 0)
    {
      if (bounded_x < 0)
        bounded_x = 0;
      if (bounded_y < 0)
        bounded_y = 0;
      if (bounded_x > host_w - 1)
        bounded_x = host_w - 1;
      if (bounded_y > host_h - 1)
        bounded_y = host_h - 1;

      if (bounded_width < 1)
        bounded_width = 1;
      if (bounded_height < 1)
        bounded_height = 1;

      if (bounded_x + bounded_width > host_w)
        bounded_width = MAX(1, host_w - bounded_x);
      if (bounded_y + bounded_height > host_h)
        bounded_height = MAX(1, host_h - bounded_y);
    }
  }

  if (instance->has_applied_screen_bounds &&
      instance->last_screen_x == bounded_x &&
      instance->last_screen_y == bounded_y &&
      instance->last_screen_width == bounded_width &&
      instance->last_screen_height == bounded_height)
  {
    return;
  }

  instance->has_applied_screen_bounds = TRUE;
  instance->last_screen_x = bounded_x;
  instance->last_screen_y = bounded_y;
  instance->last_screen_width = bounded_width;
  instance->last_screen_height = bounded_height;

  gtk_widget_set_halign(instance->container, GTK_ALIGN_START);
  gtk_widget_set_valign(instance->container, GTK_ALIGN_START);
  // Positioning is now owned entirely by the "get-child-position" signal
  // (on_get_child_position), which returns an absolute GdkRectangle to GTK.
  // Margins MUST be zero here because GTK applies them INSIDE the allocated
  // rectangle returned by that signal, which would produce a double-offset:
  //   allocated.x = bounded_x  (from on_get_child_position)
  //   effective content start = bounded_x + margin_start = 2*bounded_x  (WRONG)
  // That subtraction caused "width -82 and height 99" GTK warnings when
  // bounded_x > bounded_width (e.g. 282 > 200 → -82).
  gtk_widget_set_margin_start(instance->container, 0);
  gtk_widget_set_margin_top(instance->container, 0);
  gtk_widget_set_size_request(instance->container, bounded_width, bounded_height);

  if (instance->webkit_view)
  {
    GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);
    if (web_view_widget)
    {
      gtk_widget_set_hexpand(web_view_widget, TRUE);
      gtk_widget_set_vexpand(web_view_widget, TRUE);
      gtk_widget_set_size_request(web_view_widget, bounded_width, bounded_height);
    }
  }

  // Queue resize on the embedding overlay (not just the container) so GTK
  // invokes on_get_child_position immediately with the updated cached bounds,
  // providing exact rather than natural-size allocation.
  if (instance->embedding_overlay && GTK_IS_WIDGET(instance->embedding_overlay))
    gtk_widget_queue_resize(GTK_WIDGET(instance->embedding_overlay));
  else
    gtk_widget_queue_resize(instance->container);

  g_print("🐧 %s: %dx%d @ embedded(%d,%d) (view_id: %ld)\n",
          log_prefix, bounded_width, bounded_height, bounded_x, bounded_y, instance->view_id);
}

static void apply_overlay_screen_bounds(
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

// GtkOverlay::get-child-position signal handler.
//
// By default GtkOverlay allocates overlay children their NATURAL size (not
// their size-request minimum).  WebKitWebView's natural width is the intrinsic
// page width (e.g. 600 px for a fixed-width HTML email), which overrides
// gtk_widget_set_size_request() and causes the native surface to spill into
// the Sentria AI-panel area.
//
// This signal lets us return an EXACT GdkRectangle for our container, fully
// bypassing GTK's natural-size algorithm and guaranteeing pixel-perfect bounds.
static gboolean on_get_child_position(
    GtkOverlay *overlay,
    GtkWidget *widget,
    GdkRectangle *allocation,
    gpointer user_data)
{
  WebViewOverlayWindow *instance = (WebViewOverlayWindow *)user_data;
  // Only handle our own container; return FALSE for all other overlay children.
  if (!instance || widget != instance->container || !instance->has_applied_screen_bounds)
    return FALSE;

  allocation->x = instance->last_screen_x;
  allocation->y = instance->last_screen_y;
  allocation->width = MAX(1, instance->last_screen_width);
  allocation->height = MAX(1, instance->last_screen_height);
  return TRUE; // exact allocation provided – GTK skips default sizing
}

WebViewOverlayWindow *webview_overlay_window_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlView *flutter_view,
    WebViewWindowMode window_mode,
    FlValue *initial_settings_map_or_null)
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
  instance->has_applied_screen_bounds = FALSE;
  instance->last_screen_x = 0;
  instance->last_screen_y = 0;
  instance->last_screen_width = 0;
  instance->last_screen_height = 0;

  if (!flutter_view)
  {
    g_print("⚠️ Flutter view is null, cannot create overlay window\n");
    g_free(instance);
    return nullptr;
  }

  // Get the FlView's widget
  GtkWidget *flutter_widget = GTK_WIDGET(flutter_view);
  if (!flutter_widget)
  {
    g_print("⚠️ Flutter widget is null\n");
    g_free(instance);
    return nullptr;
  }

  // Get the parent window
  GtkWidget *toplevel = gtk_widget_get_toplevel(flutter_widget);
  if (!toplevel || !GTK_IS_WINDOW(toplevel))
  {
    g_print("⚠️ Cannot get toplevel window\n");
    g_free(instance);
    return nullptr;
  }
  instance->parent_window = GTK_WINDOW(toplevel);

  GtkWidget *flutter_parent = gtk_widget_get_parent(flutter_widget);
  if (instance->window_mode == WEBVIEW_WINDOW_MODE_OVERLAY &&
      flutter_parent && GTK_IS_OVERLAY(flutter_parent))
  {
    // True embedded path: keep WebKitGTK inside the same toplevel window.
    instance->embedded_widget_mode = TRUE;
    instance->embedding_overlay = flutter_parent;
    instance->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(instance->container, GTK_ALIGN_START);
    gtk_widget_set_valign(instance->container, GTK_ALIGN_START);
    gtk_widget_set_hexpand(instance->container, FALSE);
    gtk_widget_set_vexpand(instance->container, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(instance->embedding_overlay), instance->container);
    gtk_overlay_set_overlay_pass_through(
        GTK_OVERLAY(instance->embedding_overlay), instance->container, FALSE);
    gtk_widget_hide(instance->container);

    // Connect get-child-position so we can provide EXACT bounds to GtkOverlay,
    // overriding the natural-size algorithm that would otherwise let WebKit's
    // intrinsic page width expand the container beyond our intended clip region.
    instance->child_position_handler_id = g_signal_connect(
        G_OBJECT(instance->embedding_overlay), "get-child-position",
        G_CALLBACK(on_get_child_position), instance);
  }

  // Create window based on mode (fallback popup path)
  if (!instance->embedded_widget_mode &&
      instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    // Separate top-level window (like thunderbird)
    instance->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(instance->window, "Email Viewer");

    // Get parent window size to match height
    gint parent_width = 800, parent_height = 800;
    if (instance->parent_window)
    {
      gtk_window_get_size(instance->parent_window, &parent_width, &parent_height);
    }

    // Default size: 800 width, match parent height
    gtk_window_set_default_size(instance->window, 800, parent_height);
    gtk_window_set_resizable(instance->window, TRUE);
    gtk_window_set_decorated(instance->window, TRUE);
    gtk_window_set_deletable(instance->window, TRUE);
    gtk_window_set_keep_above(instance->window, TRUE);

    // Set as transient for main window so compositor keeps them grouped (and on X11 we can position).
    gtk_window_set_transient_for(instance->window, instance->parent_window);
    g_print("🐧 Separate viewer mode active (X11)\n");

    // Don't skip taskbar - user should see it in taskbar
    gtk_window_set_skip_taskbar_hint(instance->window, FALSE);
    gtk_window_set_skip_pager_hint(instance->window, FALSE);
  }
  else if (!instance->embedded_widget_mode)
  {
    // Overlay popup window (default)
    instance->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
    // Make window non-decorated (no title bar) and non-resizable
    gtk_window_set_decorated(instance->window, FALSE);
    gtk_window_set_resizable(instance->window, FALSE);
    // Make window skip taskbar and pager
    gtk_window_set_skip_taskbar_hint(instance->window, TRUE);
    gtk_window_set_skip_pager_hint(instance->window, TRUE);
    // Set as transient window of parent (stays on top and moves with parent)
    gtk_window_set_transient_for(instance->window, instance->parent_window);
    gtk_window_set_modal(instance->window, FALSE); // Explicitly non-modal
    gtk_window_set_type_hint(instance->window, GDK_WINDOW_TYPE_HINT_UTILITY);
  }

  // Connect to parent window configure only for popup windows.
  if (!instance->embedded_widget_mode)
  {
    instance->parent_configure_handler_id = g_signal_connect(
        G_OBJECT(instance->parent_window), "configure-event",
        G_CALLBACK(on_parent_configure_event), instance);
  }

  // In overlay mode, avoid stealing focus from the main app window
  // (this keeps titlebar/window-control interactions reliable).
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

  // Create container for WebView (popup path only).
  if (!instance->embedded_widget_mode)
  {
    instance->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(instance->window), instance->container);
  }

  // Create WebKitWebView instance
  instance->webkit_view =
      webview_webkitgtk_new(method_channel, view_id, initial_settings_map_or_null);
  GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);

  if (web_view_widget)
  {
    gtk_box_pack_start(GTK_BOX(instance->container), web_view_widget, TRUE, TRUE, 0);
  }

  // Connect delete event (popup path only)
  if (!instance->embedded_widget_mode &&
      instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    // For separate window, hide on close and notify Dart
    g_signal_connect(instance->window, "delete-event",
                     G_CALLBACK(on_window_delete_event), instance);
  }
  else if (!instance->embedded_widget_mode)
  {
    // For overlay, just hide
    g_signal_connect(instance->window, "delete-event",
                     G_CALLBACK(on_window_delete_event), instance);
  }

  // Forward keyboard shortcuts while native viewer has focus (popup path).
  if (!instance->embedded_widget_mode)
  {
    g_signal_connect(instance->window, "key-press-event",
                     G_CALLBACK(on_window_key_press_event), instance);
  }

  if (!instance->embedded_widget_mode)
  {
    // Set initial size and position
    gtk_window_set_default_size(instance->window, instance->width, instance->height);
    gtk_window_move(instance->window, instance->x, instance->y);

    // Initially hidden
    gtk_widget_hide(GTK_WIDGET(instance->window));
    g_print("🐧 Overlay window created (popup mode, view_id: %ld)\n", view_id);
  }
  else
  {
    g_print("🐧 Overlay window created (embedded mode, view_id: %ld)\n", view_id);
  }

  return instance;
}

static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  // Don't actually destroy, just hide
  gtk_widget_hide(widget);

  // Notify Dart that window was closed (for separate window mode)
  if (instance && instance->method_channel && instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    g_autoptr(FlValue) map = fl_value_new_map();
    fl_value_set_string_take(map, "closed", fl_value_new_string("true"));
    fl_method_channel_invoke_method(instance->method_channel, "onWindowClosed", map, nullptr, nullptr, nullptr);
    g_print("🐧 Notified Dart about window close\n");
  }

  return TRUE; // Prevent default destroy
}

// Handle parent window configure events (moves/resizes)
static gboolean on_parent_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance || !instance->window)
    return FALSE;

  // Only reposition if window is visible
  if (!gtk_widget_get_visible(GTK_WIDGET(instance->window)))
  {
    return FALSE;
  }

  if (instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE)
  {
    // Keep separate viewer geometry stable after first placement.
    // Reposition loops can fight compositor placement on Wayland/X11 WMs.
    return FALSE;
  }
  else
  {
    // Overlay mode is driven by Flutter setBounds updates.
    // Reapplying cached bounds here can push stale geometry during live
    // resize (old size first, then new), which causes visible jitter.
    return FALSE;
  }

  return FALSE; // Let other handlers process the event too
}

static gboolean on_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
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
  return TRUE;
}

void webview_overlay_window_destroy(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;

  g_print("🐧 Destroying overlay window (view_id: %ld)\n", instance->view_id);

  // Disconnect signal handlers from parent window
  if (instance->parent_configure_handler_id > 0 && instance->parent_window)
  {
    g_signal_handler_disconnect(G_OBJECT(instance->parent_window), instance->parent_configure_handler_id);
    instance->parent_configure_handler_id = 0;
  }

  // Disconnect the get-child-position handler from the embedding overlay
  if (instance->child_position_handler_id > 0 &&
      instance->embedding_overlay &&
      GTK_IS_WIDGET(instance->embedding_overlay))
  {
    g_signal_handler_disconnect(G_OBJECT(instance->embedding_overlay),
                                instance->child_position_handler_id);
    instance->child_position_handler_id = 0;
  }

  if (instance->webkit_view)
  {
    webview_webkitgtk_destroy(instance->webkit_view);
    instance->webkit_view = nullptr;
  }

  if (instance->window)
  {
    // Guard: the GtkWindow may have already been finalized by GTK (e.g. if the
    // parent GtkOverlay or the application window was destroyed first).
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
      // Removing from the overlay decrements the ref-count and frees the widget
      // when it reaches zero.  Do not call gtk_widget_destroy separately.
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
    if (instance->container)
    {
      gtk_widget_show_all(instance->container);
    }
    g_print("🐧 Embedded overlay shown (view_id: %ld)\n", instance->view_id);
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

  // Intentionally skip delayed reposition for separate mode to avoid repeated
  // resize/move churn after first show (pipeline now serves stable cached HTML).

  g_print("🐧 %s window shown (view_id: %ld)\n",
          instance->window_mode == WEBVIEW_WINDOW_MODE_SEPARATE ? "Separate" : "Overlay",
          instance->view_id);
}

void webview_overlay_window_hide(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;
  if (instance->embedded_widget_mode)
  {
    if (instance->container)
      gtk_widget_hide(instance->container);
  }
  else if (instance->window)
  {
    gtk_widget_hide(GTK_WIDGET(instance->window));
  }
  g_print("🐧 Overlay window hidden (view_id: %ld)\n", instance->view_id);
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
    apply_embedded_bounds(instance, x, y, width, height, "Overlay bounds (embedded)");
    return;
  }

  if (!instance->window)
    return;

  // Coordinates from Dart are window-relative (relative to FlutterView's client area)
  // Get FlutterView's client area position in screen coordinates using GdkWindow
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

        // Convert window-relative to screen coordinates by adding client area origin
        gint screen_x = client_x + x;
        gint screen_y = client_y + y;

        apply_overlay_screen_bounds(
            instance,
            screen_x,
            screen_y,
            width,
            height,
            "Overlay bounds");
        return;
      }
    }
  }

  // Fallback: use parent window position (less accurate due to decorations)
  GtkWindow *parent = instance->parent_window;
  if (parent)
  {
    gint parent_x = 0, parent_y = 0;
    gtk_window_get_position(parent, &parent_x, &parent_y);

    gint screen_x = parent_x + x;
    gint screen_y = parent_y + y;

    apply_overlay_screen_bounds(
        instance,
        screen_x,
        screen_y,
        width,
        height,
        "Overlay bounds (fallback)");
  }
  else
  {
    // No parent - use coordinates as screen coordinates (last resort)
    apply_overlay_screen_bounds(
        instance,
        x,
        y,
        width,
        height,
        "Overlay bounds (no parent)");
  }
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
    // Convert screen-absolute coordinates to Flutter-view-relative coordinates.
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
          apply_embedded_bounds(
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

    // If we cannot reliably convert screen coordinates to local embedded space,
    // keep last valid placement instead of applying a potentially huge offset.
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

  apply_overlay_screen_bounds(
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

// Position window next to main window (for separate window mode).
// main_x, main_y, main_width, main_height: if main_width > 0 and main_height > 0, use these
// as the main window rect; otherwise query from parent (GdkWindow origin + gtk_window_get_size).
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
  {
    return;
  }

  GdkDisplay *display = gdk_display_get_default();
  if (display && GDK_IS_WAYLAND_DISPLAY(display))
  {
    // Wayland compositors generally ignore client-set absolute coordinates for
    // toplevel windows. Use compositor-friendly centering + sizing only.
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
    // Prefer the Flutter view geometry (client area). This is typically more
    // accurate than querying the toplevel window size in non-maximized mode.
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
        // Keep already-populated coordinates from flutter_view when available.
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

  // Place next to the main window, but keep viewer fully on-screen.
  // This fixes the "works only when maximized" behavior when the app starts
  // in a resized position near screen edges.
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
    {
      monitor = gdk_display_get_primary_monitor(display);
    }
    if (monitor)
    {
      gdk_monitor_get_geometry(monitor, &monitor_geom);
    }
  }

  const gint monitor_left = monitor_geom.x + kScreenMarginPx;
  const gint monitor_top = monitor_geom.y + kScreenMarginPx;
  const gint monitor_right = monitor_geom.x + monitor_geom.width - kScreenMarginPx;
  const gint monitor_bottom = monitor_geom.y + monitor_geom.height - kScreenMarginPx;

  if (webview_height > (monitor_bottom - monitor_top))
  {
    webview_height = monitor_bottom - monitor_top;
  }

  const gint right_x = parent_x + parent_width + kViewerGapPx;
  const gint left_x = parent_x - webview_width - kViewerGapPx;
  const gboolean fits_right = (right_x + webview_width) <= monitor_right;
  const gboolean fits_left = left_x >= monitor_left;

  gint viewer_x = right_x;
  if (fits_right)
  {
    viewer_x = right_x;
  }
  else if (fits_left)
  {
    viewer_x = left_x;
  }
  else
  {
    // Last resort: clamp inside monitor bounds.
    if (viewer_x + webview_width > monitor_right)
    {
      viewer_x = monitor_right - webview_width;
    }
    if (viewer_x < monitor_left)
    {
      viewer_x = monitor_left;
    }
  }

  gint viewer_y = parent_y + kViewerOffsetY;
  if (viewer_y + webview_height > monitor_bottom)
  {
    viewer_y = monitor_bottom - webview_height;
  }
  if (viewer_y < monitor_top)
  {
    viewer_y = monitor_top;
  }

  gtk_window_resize(instance->window, webview_width, webview_height);
  gtk_window_move(instance->window, viewer_x, viewer_y);

  instance->width = webview_width;
  instance->height = webview_height;

  g_print("🐧 Positioned separate window: %dx%d @ (%d,%d) next to main (parent %dx%d @ (%d,%d), monitor %dx%d @ (%d,%d))\n",
          webview_width, webview_height, viewer_x, viewer_y,
          parent_width, parent_height, parent_x, parent_y,
          monitor_geom.width, monitor_geom.height, monitor_geom.x, monitor_geom.y);
}
