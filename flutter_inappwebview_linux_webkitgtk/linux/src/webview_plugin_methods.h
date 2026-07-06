#ifndef WEBVIEW_PLUGIN_METHODS_H_
#define WEBVIEW_PLUGIN_METHODS_H_

// Method-channel names for WebView plugin.
// Keep this as the single source of truth for method identifiers.

enum class WebViewPluginMethod
{
  kCreate,
  kEnsureEmailViewer,
  kOpenEmailViewer,
  kDispose,
  kSetBounds,
  kShow,
  kGetMainWindowGeometry,
  kPositionNextToMain,
  kHide,
  kLoadUrl,
  kLoadHtml,
  kEvaluateJavaScript,
  kReload,
  kGoBack,
  kCanGoBack,
  kGetCurrentUrl,
  kAddJavaScriptChannel,
  kRegisterSchemeRoute,
  kUnregisterSchemeRoute,
  kSetZoom,
  kApplySettings,
  kUnknown,
};

static constexpr const char *kMethodCreate = "create";
static constexpr const char *kMethodEnsureEmailViewer = "ensureEmailViewer";
static constexpr const char *kMethodOpenEmailViewer = "openEmailViewer";
static constexpr const char *kMethodDispose = "dispose";
static constexpr const char *kMethodSetBounds = "setBounds";
static constexpr const char *kMethodShow = "show";
static constexpr const char *kMethodGetMainWindowGeometry = "getMainWindowGeometry";
static constexpr const char *kMethodPositionNextToMain = "positionNextToMain";
static constexpr const char *kMethodHide = "hide";
static constexpr const char *kMethodLoadUrl = "loadUrl";
static constexpr const char *kMethodLoadHtml = "loadHtml";
static constexpr const char *kMethodEvaluateJavaScript = "evaluateJavaScript";
static constexpr const char *kMethodReload = "reload";
static constexpr const char *kMethodGoBack = "goBack";
static constexpr const char *kMethodCanGoBack = "canGoBack";
static constexpr const char *kMethodGetCurrentUrl = "getCurrentUrl";
static constexpr const char *kMethodAddJavaScriptChannel = "addJavaScriptChannel";
static constexpr const char *kMethodRegisterSchemeRoute = "registerSchemeRoute";
static constexpr const char *kMethodUnregisterSchemeRoute = "unregisterSchemeRoute";
static constexpr const char *kMethodSetZoom = "setZoom";
/// Optional map under key "settings" (same shape as create/ensure) or full map as args.
/// Applies WebKitGTK policies for any embedder; unknown keys are ignored.
static constexpr const char *kMethodApplySettings = "applySettings";

#endif // WEBVIEW_PLUGIN_METHODS_H_
