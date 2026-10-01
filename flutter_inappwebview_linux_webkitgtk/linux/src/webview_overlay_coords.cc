#include "webview_overlay_coords.h"

#include "webview_overlay_debug.h"

#include <gtk/gtk.h>
#include <math.h>

gboolean webview_overlay_view_size_is_stale(gdouble flutter_view_w,
                                            gdouble flutter_view_h,
                                            gint flview_alloc_w,
                                            gint flview_alloc_h)
{
  if (flutter_view_w <= 1.0 || flutter_view_h <= 1.0)
    return FALSE;
  if (flview_alloc_w <= 0 || flview_alloc_h <= 0)
    return FALSE;
  return fabs(flutter_view_w - (gdouble)flview_alloc_w) >
             WEBVIEW_OVERLAY_STALE_VIEW_EPSILON ||
         fabs(flutter_view_h - (gdouble)flview_alloc_h) >
             WEBVIEW_OVERLAY_STALE_VIEW_EPSILON;
}

void webview_overlay_convert_flutter_bounds(
    WebViewOverlayWindow *instance,
    gdouble flutter_x,
    gdouble flutter_y,
    gdouble flutter_w,
    gdouble flutter_h,
    gdouble flutter_view_w,
    gdouble flutter_view_h,
    gint *out_x,
    gint *out_y,
    gint *out_w,
    gint *out_h,
    gboolean *out_stale)
{
  gint origin_x = 0;
  gint origin_y = 0;
  gboolean stale = FALSE;

  GtkWidget *flutter_widget =
      instance && instance->flutter_view ? GTK_WIDGET(instance->flutter_view)
                                         : nullptr;
  GtkWidget *host = instance ? instance->embedding_overlay : nullptr;

  if (flutter_widget && host &&
      gtk_widget_get_realized(flutter_widget) &&
      gtk_widget_get_realized(host))
  {
    if (!gtk_widget_translate_coordinates(
            flutter_widget, host, 0, 0, &origin_x, &origin_y))
    {
      origin_x = 0;
      origin_y = 0;
    }
    stale = webview_overlay_view_size_is_stale(
        flutter_view_w,
        flutter_view_h,
        gtk_widget_get_allocated_width(flutter_widget),
        gtk_widget_get_allocated_height(flutter_widget));
  }

  const gint left = (gint)lround((gdouble)origin_x + flutter_x);
  const gint top = (gint)lround((gdouble)origin_y + flutter_y);
  const gint right = (gint)lround((gdouble)origin_x + flutter_x + flutter_w);
  const gint bottom = (gint)lround((gdouble)origin_y + flutter_y + flutter_h);

  coord_print(
      "🐧 Coord map: flutter(%.1f,%.1f %.1fx%.1f) viewLogical=%.1fx%.1f "
      "origin=%d,%d stale=%d -> overlay(%d,%d %dx%d)\n",
      flutter_x, flutter_y, flutter_w, flutter_h,
      flutter_view_w, flutter_view_h,
      origin_x, origin_y, stale,
      left, top, right - left, bottom - top);

  if (out_x)
    *out_x = left;
  if (out_y)
    *out_y = top;
  if (out_w)
    *out_w = MAX(1, right - left);
  if (out_h)
    *out_h = MAX(1, bottom - top);
  if (out_stale)
    *out_stale = stale;
}

gboolean webview_overlay_intersect_host(gint x,
                                        gint y,
                                        gint w,
                                        gint h,
                                        gint host_w,
                                        gint host_h,
                                        gint *out_x,
                                        gint *out_y,
                                        gint *out_w,
                                        gint *out_h)
{
  const gint left = MAX(0, x);
  const gint top = MAX(0, y);
  const gint right = MIN(host_w, x + w);
  const gint bottom = MIN(host_h, y + h);
  const gboolean non_empty = right > left && bottom > top;

  if (out_x)
    *out_x = non_empty ? left : 0;
  if (out_y)
    *out_y = non_empty ? top : 0;
  if (out_w)
    *out_w = non_empty ? right - left : 1;
  if (out_h)
    *out_h = non_empty ? bottom - top : 1;
  return non_empty;
}
