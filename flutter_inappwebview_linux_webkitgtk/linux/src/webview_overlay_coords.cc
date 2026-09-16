#include "webview_overlay_coords.h"

#include "webview_overlay_debug.h"

#include <gtk/gtk.h>
#include <math.h>

gdouble webview_overlay_axis_scale(gdouble flutter_logical,
                                   gint view_px,
                                   gdouble dpr)
{
  if (flutter_logical <= 1.0 || view_px <= 0)
    return dpr;
  const gdouble from_view = (gdouble)view_px / flutter_logical;
  const gdouble tolerance = MAX(0.08, fabs(dpr) * 0.15);
  if (fabs(from_view - dpr) <= tolerance)
    return from_view;
  return dpr;
}

void webview_overlay_convert_flutter_bounds(
    WebViewOverlayWindow *instance,
    gdouble flutter_x,
    gdouble flutter_y,
    gdouble flutter_w,
    gdouble flutter_h,
    gdouble flutter_view_w,
    gdouble flutter_view_h,
    gdouble device_pixel_ratio,
    gint *out_x,
    gint *out_y,
    gint *out_w,
    gint *out_h)
{
  gint overlay_x = (gint)lround(flutter_x);
  gint overlay_y = (gint)lround(flutter_y);
  gint overlay_w = (gint)lround(flutter_w);
  gint overlay_h = (gint)lround(flutter_h);

  GtkWidget *flutter_widget =
      instance && instance->flutter_view ? GTK_WIDGET(instance->flutter_view)
                                         : nullptr;
  GtkWidget *host = instance ? instance->embedding_overlay : nullptr;

  if (flutter_widget && host &&
      gtk_widget_get_realized(flutter_widget) &&
      gtk_widget_get_realized(host))
  {
    const gint view_px_w = gtk_widget_get_allocated_width(flutter_widget);
    const gint view_px_h = gtk_widget_get_allocated_height(flutter_widget);

    const gdouble dpr =
        device_pixel_ratio > 0.01 ? device_pixel_ratio : 1.0;
    const gdouble scale_x = webview_overlay_axis_scale(flutter_view_w, view_px_w, dpr);
    const gdouble scale_y = webview_overlay_axis_scale(flutter_view_h, view_px_h, dpr);

    gint origin_x = 0;
    gint origin_y = 0;
    if (!gtk_widget_translate_coordinates(
            flutter_widget, host, 0, 0, &origin_x, &origin_y))
    {
      origin_x = 0;
      origin_y = 0;
    }

    {
      const gdouble left = (gdouble)origin_x + flutter_x * scale_x;
      const gdouble top = (gdouble)origin_y + flutter_y * scale_y;
      const gdouble right = left + flutter_w * scale_x;
      const gdouble bottom = top + flutter_h * scale_y;
      overlay_x = (gint)lround(left);
      overlay_y = (gint)lround(top);
      const gint right_i = (gint)lround(right);
      const gint bottom_i = (gint)lround(bottom);
      overlay_w = MAX(1, right_i - overlay_x);
      overlay_h = MAX(1, bottom_i - overlay_y);
    }

    coord_print(
        "🐧 Coord map: flutter(%.1f,%.1f %.1fx%.1f) viewLogical=%.1fx%.1f "
        "flAlloc=%dx%d host=%dx%d origin=%d,%d scale=%.3fx%.3f "
        "-> overlay(%d,%d %dx%d) dpr=%.2f\n",
        flutter_x,
        flutter_y,
        flutter_w,
        flutter_h,
        flutter_view_w,
        flutter_view_h,
        view_px_w,
        view_px_h,
        gtk_widget_get_allocated_width(host),
        gtk_widget_get_allocated_height(host),
        origin_x,
        origin_y,
        scale_x,
        scale_y,
        overlay_x,
        overlay_y,
        overlay_w,
        overlay_h,
        device_pixel_ratio);
  }

  if (out_x)
    *out_x = overlay_x;
  if (out_y)
    *out_y = overlay_y;
  if (out_w)
    *out_w = MAX(1, overlay_w);
  if (out_h)
    *out_h = MAX(1, overlay_h);
}
