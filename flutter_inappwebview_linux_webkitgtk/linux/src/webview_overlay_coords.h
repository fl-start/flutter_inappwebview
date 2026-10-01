#ifndef WEBVIEW_OVERLAY_COORDS_H_
#define WEBVIEW_OVERLAY_COORDS_H_

#include <glib.h>
#include "webview_overlay_window.h"

G_BEGIN_DECLS

// Max |Flutter logical view size - FlView allocation| (GTK px) before a
// payload is treated as measured against stale metrics.
#define WEBVIEW_OVERLAY_STALE_VIEW_EPSILON 1.5

// TRUE when Dart's logical view size disagrees with the FlView allocation.
// GTK3 FlView allocation is always in application (logical) pixels and the
// Flutter engine reports physical = allocation * integer scale factor, so in
// steady state the two are equal; any mismatch means Dart has not yet laid out
// against the latest window metrics (maximize / resize allocate lag).
gboolean webview_overlay_view_size_is_stale(gdouble flutter_view_w,
                                            gdouble flutter_view_h,
                                            gint flview_alloc_w,
                                            gint flview_alloc_h);

// Convert Flutter-view-local logical coordinates to GtkOverlay allocation
// space: identity scale plus the FlView origin inside the GtkOverlay. Edges are
// rounded independently and size derived from rounded edges. Sets *out_stale
// when the payload was measured against stale Flutter metrics.
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
    gboolean *out_stale);

// Intersect an overlay-relative rect with the host allocation. Returns FALSE
// when the intersection is empty (outputs then describe a 1x1 rect at 0,0).
gboolean webview_overlay_intersect_host(gint x,
                                        gint y,
                                        gint w,
                                        gint h,
                                        gint host_w,
                                        gint host_h,
                                        gint *out_x,
                                        gint *out_y,
                                        gint *out_w,
                                        gint *out_h);

G_END_DECLS

#endif // WEBVIEW_OVERLAY_COORDS_H_
