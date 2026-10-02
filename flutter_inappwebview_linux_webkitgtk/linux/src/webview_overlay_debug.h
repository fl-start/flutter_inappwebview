#ifndef WEBVIEW_OVERLAY_DEBUG_H_
#define WEBVIEW_OVERLAY_DEBUG_H_

#include <glib.h>

// Verbose native coordinate traces (off by default; Dart logs layout regions).
// Do not redefine g_print: creation/show/error diagnostics must stay visible.
#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define WEBVIEW_ENABLE_DEBUG_PRINTS 0
#endif

#if WEBVIEW_ENABLE_DEBUG_PRINTS
#define coord_print(...) g_print(__VA_ARGS__)
#else
#define coord_print(...) ((void)0)
#endif

#endif // WEBVIEW_OVERLAY_DEBUG_H_
