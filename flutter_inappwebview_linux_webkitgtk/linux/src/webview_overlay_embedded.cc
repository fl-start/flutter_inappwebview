#include "webview_overlay_embedded.h"

#include "webview_overlay_coords.h"
#include "webview_overlay_debug.h"

// Trailing debounce for onHostLayoutChanged. Long enough to coalesce a
// size-allocate burst, short enough that maximize settles within a few frames.
static constexpr guint kHostLayoutNotifyDebounceMs = 48;

void webview_overlay_clear_source(guint *id)
{
  if (id && *id != 0)
  {
    g_source_remove(*id);
    *id = 0;
  }
}

void webview_overlay_cancel_idle_sources(WebViewOverlayWindow *instance)
{
  if (!instance)
    return;
  webview_overlay_clear_source(&instance->idle_force_bounds_source_id);
  webview_overlay_clear_source(&instance->timeout_force_bounds_32_source_id);
  webview_overlay_clear_source(&instance->timeout_force_bounds_120_source_id);
  webview_overlay_clear_source(&instance->idle_raise_source_id);
  webview_overlay_clear_source(&instance->timeout_raise_32_source_id);
  webview_overlay_clear_source(&instance->host_layout_notify_source_id);
}

GtkWidget *webview_overlay_find_gtk_overlay_ancestor(GtkWidget *widget)
{
  for (GtkWidget *w = widget; w != nullptr; w = gtk_widget_get_parent(w))
  {
    if (GTK_IS_OVERLAY(w))
      return w;
  }
  return nullptr;
}

// raise_embedded_overlay_child — Impeller FlView GL can sit above GtkOverlay
// children; reorder + gdk_window_raise without fighting placement.
void webview_overlay_raise_embedded_child(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->embedded_widget_mode || !instance->container)
    return;
  if (instance->embedding_overlay &&
      GTK_IS_OVERLAY(instance->embedding_overlay) &&
      gtk_widget_get_parent(instance->container) == instance->embedding_overlay)
  {
    gtk_overlay_reorder_overlay(
        GTK_OVERLAY(instance->embedding_overlay), instance->container, -1);
  }
  if (!gtk_widget_get_realized(instance->container))
    return;
  GdkWindow *child_win = gtk_widget_get_window(instance->container);
  if (child_win)
    gdk_window_raise(child_win);
  if (!instance->webkit_view)
    return;
  GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);
  if (!web_view_widget || !gtk_widget_get_realized(web_view_widget))
    return;
  GdkWindow *web_win = gtk_widget_get_window(web_view_widget);
  if (web_win && web_win != child_win)
    gdk_window_raise(web_win);
}

static gboolean idle_raise_embedded_overlay_child(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->idle_raise_source_id = 0;
  if (!instance->container)
    return G_SOURCE_REMOVE;
  webview_overlay_raise_embedded_child(instance);
  return G_SOURCE_REMOVE;
}

static gboolean timeout_raise_embedded_overlay_child(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->timeout_raise_32_source_id = 0;
  if (!instance->container)
    return G_SOURCE_REMOVE;
  webview_overlay_raise_embedded_child(instance);
  return G_SOURCE_REMOVE;
}

// GTK recomputes a widget clip on every allocation, so a clip set from the
// bounds-apply path is overwritten by the next size-allocate. Set it from the
// widget's own size-allocate (run after the default handler) instead.
void webview_overlay_on_embedded_widget_size_allocate(GtkWidget *widget,
                                                      GtkAllocation *allocation,
                                                      gpointer user_data)
{
  (void)user_data;
  if (!widget || !allocation || allocation->width <= 0 || allocation->height <= 0)
    return;
  GdkRectangle clip = {0, 0, allocation->width, allocation->height};
  gtk_widget_set_clip(widget, &clip);
}

static void connect_clip_on_allocate(GtkWidget *widget)
{
  if (!widget || g_object_get_data(G_OBJECT(widget), "scomm-clip-on-allocate"))
    return;
  g_object_set_data(G_OBJECT(widget), "scomm-clip-on-allocate",
                    GINT_TO_POINTER(1));
  g_signal_connect_after(
      widget, "size-allocate",
      G_CALLBACK(webview_overlay_on_embedded_widget_size_allocate), nullptr);
}

void webview_overlay_on_embedded_container_realize(GtkWidget *widget,
                                                   gpointer user_data)
{
  (void)widget;
  webview_overlay_raise_embedded_child(
      static_cast<WebViewOverlayWindow *>(user_data));
}

static void force_overlay_child_gdk_window_bounds(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->container || !instance->embedding_overlay)
    return;
  if (!instance->has_applied_overlay_bounds)
    return;
  if (!gtk_widget_get_visible(instance->container))
    return;
  if (!gtk_widget_get_realized(instance->container) ||
      !gtk_widget_get_realized(instance->embedding_overlay))
    return;

  const gint win_w = MAX(1, instance->last_overlay_alloc_width);
  const gint win_h = MAX(1, instance->last_overlay_alloc_height);

  // Trust GtkOverlay "get-child-position". last_overlay_alloc_* are
  // overlay-relative. Do NOT gdk_window_move_resize into toplevel space: that
  // fights GTK and shifts the WebView off the placeholder.
  if (instance->child_position_handler_id != 0)
  {
    gtk_widget_set_size_request(instance->container, win_w, win_h);
    gtk_widget_queue_resize(instance->embedding_overlay);
    gtk_widget_queue_allocate(instance->embedding_overlay);
    coord_print(
        "🐧 get-child-position assert signal=(%d,%d %dx%d) view_id=%ld\n",
        instance->last_overlay_alloc_x,
        instance->last_overlay_alloc_y,
        win_w,
        win_h,
        instance->view_id);
    webview_overlay_raise_embedded_child(instance);
    return;
  }

  GtkAllocation alloc = {
      .x = instance->last_overlay_alloc_x,
      .y = instance->last_overlay_alloc_y,
      .width = win_w,
      .height = win_h,
  };
  gtk_widget_size_allocate(instance->container, &alloc);
  coord_print(
      "🐧 No get-child-position; size_allocate widget @ (%d,%d %dx%d) "
      "view_id=%ld\n",
      alloc.x,
      alloc.y,
      alloc.width,
      alloc.height,
      instance->view_id);
  webview_overlay_raise_embedded_child(instance);
}

static gboolean idle_force_overlay_child_bounds(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->idle_force_bounds_source_id = 0;
  if (!instance->container)
    return G_SOURCE_REMOVE;
  force_overlay_child_gdk_window_bounds(instance);
  return G_SOURCE_REMOVE;
}

static gboolean timeout_force_overlay_child_bounds_32(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->timeout_force_bounds_32_source_id = 0;
  if (!instance->container)
    return G_SOURCE_REMOVE;
  force_overlay_child_gdk_window_bounds(instance);
  return G_SOURCE_REMOVE;
}

static gboolean timeout_force_overlay_child_bounds_120(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->timeout_force_bounds_120_source_id = 0;
  if (!instance->container)
    return G_SOURCE_REMOVE;
  force_overlay_child_gdk_window_bounds(instance);
  return G_SOURCE_REMOVE;
}

static void schedule_force_overlay_child_bounds(WebViewOverlayWindow *instance)
{
  webview_overlay_clear_source(&instance->idle_force_bounds_source_id);
  webview_overlay_clear_source(&instance->timeout_force_bounds_32_source_id);
  webview_overlay_clear_source(&instance->timeout_force_bounds_120_source_id);
  instance->idle_force_bounds_source_id =
      g_idle_add(idle_force_overlay_child_bounds, instance);
  instance->timeout_force_bounds_32_source_id =
      g_timeout_add(32, timeout_force_overlay_child_bounds_32, instance);
  instance->timeout_force_bounds_120_source_id =
      g_timeout_add(120, timeout_force_overlay_child_bounds_120, instance);
}

void webview_overlay_schedule_raise(WebViewOverlayWindow *instance)
{
  webview_overlay_raise_embedded_child(instance);
  webview_overlay_clear_source(&instance->idle_raise_source_id);
  webview_overlay_clear_source(&instance->timeout_raise_32_source_id);
  instance->idle_raise_source_id =
      g_idle_add(idle_raise_embedded_overlay_child, instance);
  instance->timeout_raise_32_source_id =
      g_timeout_add(32, timeout_raise_embedded_overlay_child, instance);
}

void webview_overlay_sync_embedded_visibility(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->embedded_widget_mode || !instance->container)
    return;

  // Showing before the first setBounds would let GtkOverlay place the child
  // at its default (0,0) natural size — a flash over the inbox.
  const gboolean should_show = instance->wants_visible &&
                               instance->has_applied_overlay_bounds &&
                               !instance->hidden_by_host_clip;
  const gboolean shown = gtk_widget_get_visible(instance->container);
  if (should_show && !shown)
  {
    gtk_widget_show_all(instance->container);
    webview_overlay_schedule_raise(instance);
  }
  else if (!should_show && shown)
  {
    gtk_widget_hide(instance->container);
  }
}

void webview_overlay_apply_embedded_bounds(
    WebViewOverlayWindow *instance,
    gint x,
    gint y,
    gint width,
    gint height,
    const gchar *log_prefix)
{
  if (!instance || !instance->embedded_widget_mode || !instance->container)
    return;

  gint bounded_x = x;
  gint bounded_y = y;
  gint bounded_width = MAX(1, width);
  gint bounded_height = MAX(1, height);
  gboolean inside_host = TRUE;

  GtkWidget *host = instance->embedding_overlay;
  if (host && gtk_widget_get_realized(host))
  {
    const gint host_w = gtk_widget_get_allocated_width(host);
    const gint host_h = gtk_widget_get_allocated_height(host);
    if (host_w > 0 && host_h > 0)
    {
      inside_host = webview_overlay_intersect_host(
          x, y, bounded_width, bounded_height, host_w, host_h,
          &bounded_x, &bounded_y, &bounded_width, &bounded_height);
    }
  }
  instance->hidden_by_host_clip = !inside_host;

  const gboolean unchanged =
      instance->has_applied_overlay_bounds &&
      instance->last_overlay_alloc_x == bounded_x &&
      instance->last_overlay_alloc_y == bounded_y &&
      instance->last_overlay_alloc_width == bounded_width &&
      instance->last_overlay_alloc_height == bounded_height;

  instance->has_applied_overlay_bounds = TRUE;
  instance->last_overlay_alloc_x = bounded_x;
  instance->last_overlay_alloc_y = bounded_y;
  instance->last_overlay_alloc_width = bounded_width;
  instance->last_overlay_alloc_height = bounded_height;

  if (unchanged)
  {
    webview_overlay_sync_embedded_visibility(instance);
    force_overlay_child_gdk_window_bounds(instance);
    return;
  }

  gtk_widget_set_halign(instance->container, GTK_ALIGN_START);
  gtk_widget_set_valign(instance->container, GTK_ALIGN_START);
  gtk_widget_set_margin_start(instance->container, 0);
  gtk_widget_set_margin_end(instance->container, 0);
  gtk_widget_set_margin_top(instance->container, 0);
  gtk_widget_set_margin_bottom(instance->container, 0);
  gtk_widget_set_size_request(instance->container, bounded_width, bounded_height);

  if (instance->webkit_view)
  {
    GtkWidget *web_view_widget = webview_webkitgtk_get_widget(instance->webkit_view);
    if (web_view_widget)
    {
      gtk_widget_set_hexpand(web_view_widget, TRUE);
      gtk_widget_set_vexpand(web_view_widget, TRUE);
      gtk_widget_set_size_request(web_view_widget, bounded_width, bounded_height);
      connect_clip_on_allocate(web_view_widget);
    }
  }

  connect_clip_on_allocate(instance->container);

  if (host && GTK_IS_WIDGET(host))
    gtk_widget_queue_resize(host);
  else
    gtk_widget_queue_resize(instance->container);

  webview_overlay_sync_embedded_visibility(instance);
  force_overlay_child_gdk_window_bounds(instance);
  schedule_force_overlay_child_bounds(instance);

  coord_print("🐧 %s: %dx%d @ embedded(%d,%d) inside_host=%d (view_id: %ld)\n",
              log_prefix, bounded_width, bounded_height, bounded_x, bounded_y,
              inside_host, instance->view_id);
}

static gboolean host_layout_notify_cb(gpointer user_data)
{
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance)
    return G_SOURCE_REMOVE;
  instance->host_layout_notify_source_id = 0;
  if (!instance->method_channel || !instance->embedded_widget_mode)
    return G_SOURCE_REMOVE;

  g_autoptr(FlValue) map = fl_value_new_map();
  fl_value_set_string_take(
      map, "viewId", fl_value_new_int((int64_t)instance->view_id));
  fl_method_channel_invoke_method(
      instance->method_channel,
      "onHostLayoutChanged",
      map,
      nullptr,
      nullptr,
      nullptr);
  return G_SOURCE_REMOVE;
}

void webview_overlay_schedule_host_layout_changed(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->method_channel || !instance->embedded_widget_mode)
    return;
  webview_overlay_clear_source(&instance->host_layout_notify_source_id);
  instance->host_layout_notify_source_id =
      g_timeout_add(kHostLayoutNotifyDebounceMs, host_layout_notify_cb, instance);
}

// FlView, GtkOverlay and toplevel size-allocate all land here. Dedupe on the
// two allocations that define overlay geometry; the toplevel allocation
// includes CSD chrome and would never compare equal to either.
static void note_host_layout(WebViewOverlayWindow *instance)
{
  if (!instance || !instance->embedded_widget_mode)
    return;

  GtkWidget *flview =
      instance->flutter_view ? GTK_WIDGET(instance->flutter_view) : nullptr;
  GtkWidget *overlay = instance->embedding_overlay;
  const gint fw = flview ? gtk_widget_get_allocated_width(flview) : 0;
  const gint fh = flview ? gtk_widget_get_allocated_height(flview) : 0;
  const gint ow = overlay ? gtk_widget_get_allocated_width(overlay) : 0;
  const gint oh = overlay ? gtk_widget_get_allocated_height(overlay) : 0;

  if (instance->has_reported_host_layout &&
      instance->last_reported_flview_width == fw &&
      instance->last_reported_flview_height == fh &&
      instance->last_reported_overlay_width == ow &&
      instance->last_reported_overlay_height == oh)
    return;

  instance->has_reported_host_layout = TRUE;
  instance->last_reported_flview_width = fw;
  instance->last_reported_flview_height = fh;
  instance->last_reported_overlay_width = ow;
  instance->last_reported_overlay_height = oh;
  webview_overlay_schedule_host_layout_changed(instance);
}

void webview_overlay_on_host_size_allocate(GtkWidget *widget,
                                           GtkAllocation *allocation,
                                           gpointer user_data)
{
  (void)widget;
  (void)allocation;
  note_host_layout(static_cast<WebViewOverlayWindow *>(user_data));
}

gboolean webview_overlay_on_get_child_position(GtkOverlay *overlay,
                                               GtkWidget *widget,
                                               GdkRectangle *allocation,
                                               gpointer user_data)
{
  (void)overlay;
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (!instance || widget != instance->container ||
      !instance->has_applied_overlay_bounds)
  {
    coord_print(
        "🐧 get-child-position skip (ours=%d applied=%d) view_id=%ld\n",
        (instance && widget == instance->container) ? 1 : 0,
        (instance && instance->has_applied_overlay_bounds) ? 1 : 0,
        instance ? instance->view_id : -1);
    return FALSE;
  }

  allocation->x = instance->last_overlay_alloc_x;
  allocation->y = instance->last_overlay_alloc_y;
  allocation->width = MAX(1, instance->last_overlay_alloc_width);
  allocation->height = MAX(1, instance->last_overlay_alloc_height);
  coord_print(
      "🐧 get-child-position -> (%d,%d %dx%d) view_id=%ld\n",
      allocation->x,
      allocation->y,
      allocation->width,
      allocation->height,
      instance->view_id);
  return TRUE;
}

gboolean webview_overlay_on_parent_configure_event(GtkWidget *widget,
                                                   GdkEventConfigure *event,
                                                   gpointer user_data)
{
  (void)widget;
  (void)event;
  WebViewOverlayWindow *instance = static_cast<WebViewOverlayWindow *>(user_data);
  if (instance && instance->embedded_widget_mode)
    note_host_layout(instance);
  return FALSE;
}
