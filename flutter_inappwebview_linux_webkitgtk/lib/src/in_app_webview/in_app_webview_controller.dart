import 'dart:collection';

import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import '../overlay/webview_controller_webkitgtk.dart';
import '../overlay/webkitgtk_keep_alive_pool.dart';

class LinuxWebKitGtkInAppWebViewControllerCreationParams
    extends PlatformInAppWebViewControllerCreationParams {
  const LinuxWebKitGtkInAppWebViewControllerCreationParams({
    required super.id,
    super.webviewParams,
    this.nativeController,
  });

  final WebViewControllerWebKitGTK? nativeController;

  factory LinuxWebKitGtkInAppWebViewControllerCreationParams.fromPlatformInAppWebViewControllerCreationParams(
    PlatformInAppWebViewControllerCreationParams params, {
    WebViewControllerWebKitGTK? nativeController,
  }) {
    return LinuxWebKitGtkInAppWebViewControllerCreationParams(
      id: params.id,
      webviewParams: params.webviewParams,
      nativeController: nativeController,
    );
  }
}

class LinuxWebKitGtkInAppWebViewController extends PlatformInAppWebViewController {
  LinuxWebKitGtkInAppWebViewController(
    PlatformInAppWebViewControllerCreationParams params,
  ) : _native =
            (params is LinuxWebKitGtkInAppWebViewControllerCreationParams
                    ? params.nativeController
                    : null) ??
                WebViewControllerWebKitGTK(params.id as int),
        super.implementation(
          params is LinuxWebKitGtkInAppWebViewControllerCreationParams
              ? params
              : LinuxWebKitGtkInAppWebViewControllerCreationParams.fromPlatformInAppWebViewControllerCreationParams(
                  params,
                ),
        );

  final WebViewControllerWebKitGTK _native;
  final Map<String, Function> _handlers = HashMap<String, Function>();

  static final LinuxWebKitGtkInAppWebViewController _static =
      LinuxWebKitGtkInAppWebViewController(
        const LinuxWebKitGtkInAppWebViewControllerCreationParams(id: -1),
      );

  factory LinuxWebKitGtkInAppWebViewController.static() => _static;

  factory LinuxWebKitGtkInAppWebViewController.fromInAppBrowser(
    LinuxWebKitGtkInAppWebViewControllerCreationParams params,
  ) => LinuxWebKitGtkInAppWebViewController(params);

  @override
  Future<WebUri?> getUrl() async {
    final url = await _native.getCurrentUrl();
    return url == null ? null : WebUri(url);
  }

  @override
  Future<void> loadUrl({
    required URLRequest urlRequest,
    Uri? iosAllowingReadAccessTo,
    WebUri? allowingReadAccessTo,
  }) async {
    await _native.loadUrl(
      urlRequest.url.toString(),
      headers: urlRequest.headers,
    );
  }

  @override
  Future<void> loadData({
    required String data,
    String mimeType = 'text/html',
    String encoding = 'utf8',
    WebUri? baseUrl,
    WebUri? historyUrl,
    Uri? iosAllowingReadAccessTo,
    WebUri? allowingReadAccessTo,
    Uri? androidHistoryUrl,
  }) async {
    await _native.loadHtml(data, baseUrl: baseUrl?.toString());
  }

  @override
  Future<void> reload() => _native.reload();

  @override
  Future<void> goBack() => _native.goBack();

  @override
  Future<bool> canGoBack() => _native.canGoBack();

  @override
  Future<dynamic> evaluateJavascript({
    required String source,
    ContentWorld? contentWorld,
  }) {
    return _native.evaluateJavaScript(source);
  }

  @override
  void addJavaScriptHandler({
    required String handlerName,
    required Function callback,
  }) {
    _handlers[handlerName] = callback;
    _native.addJavaScriptChannel(handlerName);
  }

  @override
  Function? removeJavaScriptHandler({required String handlerName}) {
    return _handlers.remove(handlerName);
  }

  Future<dynamic> dispatchJavaScriptMessage(
    String handlerName,
    dynamic payload,
  ) async {
    final handler = _handlers[handlerName];
    if (handler == null) return null;
    return handler(payload is List ? payload : [payload]);
  }

  /// Overlay view id used by grabFocus / releaseFocus method-channel calls.
  @override
  dynamic getViewId() => _native.viewId;

  /// Compose focus: claim GTK keyboard focus for the TipTap WebKit surface.
  Future<void> grabFocus() => _native.grabFocus();

  /// Compose focus: return GTK keyboard focus to Flutter's FlView.
  Future<void> releaseFocus() => _native.releaseFocus();

  @override
  Future<void> disposeKeepAlive(InAppWebViewKeepAlive keepAlive) async {
    final controller = WebKitGtkKeepAlivePool.release(keepAlive.id);
    await controller?.dispose();
  }

  @override
  Future<void> dispose({bool isKeepAlive = false}) async {
    await _native.dispose(keepAlive: isKeepAlive);
    if (!isKeepAlive) _handlers.clear();
  }
}
