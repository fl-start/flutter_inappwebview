#ifndef WEBVIEW_OVERLAY_COORDS_H_
#define WEBVIEW_OVERLAY_COORDS_H_

#include <glib.h>
#include "webview_overlay_window.h"

G_BEGIN_DECLS

// Empirical axis scale: FlView allocated px / Flutter logical size, but only
// when that ratio is within max(0.08, |dpr| * 0.15) of Dart's DPR. Otherwise
// keep DPR so maximize/restore transients do not smear the overlay.
gdouble webview_overlay_axis_scale(gdouble flutter_logical,
                                   gint view_px,
                                   gdouble dpr);

// Convert Flutter-view-local logical coordinates to GtkOverlay allocation space.
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
    gint *out_h);

G_END_DECLS

#endif // WEBVIEW_OVERLAY_COORDS_H_
