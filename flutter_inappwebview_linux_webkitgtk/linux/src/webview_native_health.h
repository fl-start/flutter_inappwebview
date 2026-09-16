#ifndef WEBVIEW_NATIVE_HEALTH_H_
#define WEBVIEW_NATIVE_HEALTH_H_

#include <flutter_linux/flutter_linux.h>
#include "webview_overlay_window.h"

G_BEGIN_DECLS

void webview_native_health_set_last_error(const gchar *reason);
const gchar *webview_native_health_last_error(void);

void webview_native_health_fill_runtime(FlValue *map);

// Snapshot for a live overlay. Always includes runtime env keys.
FlValue *webview_native_health_from_overlay(WebViewOverlayWindow *instance);

// Env + last error + overlayCount. If view_id >= 0, include that overlay.
FlValue *webview_native_health_snapshot(GHashTable *overlay_windows, gint64 view_id);

G_END_DECLS

#endif // WEBVIEW_NATIVE_HEALTH_H_
