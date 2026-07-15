// ignore_for_file: deprecated_member_use

import 'package:flutter/widgets.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import '../overlay/webkitgtk_overlay_widget.dart';
import 'in_app_webview_controller.dart';

class LinuxWebKitGtkInAppWebViewWidgetCreationParams
    extends PlatformInAppWebViewWidgetCreationParams {
  LinuxWebKitGtkInAppWebViewWidgetCreationParams({
    super.controllerFromPlatform,
    super.key,
    super.layoutDirection,
    super.gestureRecognizers,
    super.headlessWebView,
    super.keepAlive,
    super.preventGestureDelay,
    super.windowId,
    super.onWebViewCreated,
    super.onLoadStart,
    super.onLoadStop,
    @Deprecated('Use onReceivedError instead') super.onLoadError,
    super.onReceivedError,
    @Deprecated('Use onReceivedHttpError instead') super.onLoadHttpError,
    super.onReceivedHttpError,
    super.onProgressChanged,
    super.onConsoleMessage,
    super.shouldOverrideUrlLoading,
    super.initialUrlRequest,
    super.initialFile,
    super.initialData,
    @Deprecated('Use initialSettings instead') super.initialOptions,
    super.initialSettings,
    super.initialUserScripts,
    super.pullToRefreshController,
    super.findInteractionController,
  });

  LinuxWebKitGtkInAppWebViewWidgetCreationParams.fromPlatformInAppWebViewWidgetCreationParams(
    PlatformInAppWebViewWidgetCreationParams params,
  ) : this(
          controllerFromPlatform: params.controllerFromPlatform,
          key: params.key,
          layoutDirection: params.layoutDirection,
          gestureRecognizers: params.gestureRecognizers,
          headlessWebView: params.headlessWebView,
          keepAlive: params.keepAlive,
          preventGestureDelay: params.preventGestureDelay,
          windowId: params.windowId,
          onWebViewCreated: params.onWebViewCreated,
          onLoadStart: params.onLoadStart,
          onLoadStop: params.onLoadStop,
          onLoadError: params.onLoadError,
          onReceivedError: params.onReceivedError,
          onLoadHttpError: params.onLoadHttpError,
          onReceivedHttpError: params.onReceivedHttpError,
          onProgressChanged: params.onProgressChanged,
          onConsoleMessage: params.onConsoleMessage,
          shouldOverrideUrlLoading: params.shouldOverrideUrlLoading,
          initialUrlRequest: params.initialUrlRequest,
          initialFile: params.initialFile,
          initialData: params.initialData,
          initialOptions: params.initialOptions,
          initialSettings: params.initialSettings,
          initialUserScripts: params.initialUserScripts,
          pullToRefreshController: params.pullToRefreshController,
          findInteractionController: params.findInteractionController,
        );
}

class LinuxWebKitGtkInAppWebViewWidget extends PlatformInAppWebViewWidget {
  LinuxWebKitGtkInAppWebViewWidget(
    PlatformInAppWebViewWidgetCreationParams params,
  ) : super.implementation(
          params is LinuxWebKitGtkInAppWebViewWidgetCreationParams
              ? params
              : LinuxWebKitGtkInAppWebViewWidgetCreationParams.fromPlatformInAppWebViewWidgetCreationParams(
                  params,
                ),
        );

  LinuxWebKitGtkInAppWebViewWidgetCreationParams get _params =>
      params as LinuxWebKitGtkInAppWebViewWidgetCreationParams;

  LinuxWebKitGtkInAppWebViewController? _controller;

  static final LinuxWebKitGtkInAppWebViewWidget _static =
      LinuxWebKitGtkInAppWebViewWidget(
        LinuxWebKitGtkInAppWebViewWidgetCreationParams(),
      );

  factory LinuxWebKitGtkInAppWebViewWidget.static() => _static;

  @override
  Widget build(BuildContext context) {
    final initialUrl = _params.initialUrlRequest?.url.toString();
    final initialData = _params.initialData;

    return WebKitGtkOverlayWidget(
      key: _params.key,
      keepAlive: _params.keepAlive,
      initialSettings: _params.initialSettings?.toMap(),
      initialUserScripts: _params.initialUserScripts,
      initialUrl: initialUrl,
      initialHtml: initialData?.data,
      initialHtmlBaseUrl: initialData?.baseUrl?.toString(),
      onLoadStart: (url) {
        final c = _controller;
        if (c != null) {
          _params.onLoadStart?.call(c, WebUri(url));
        }
      },
      onLoadStop: (url) {
        final c = _controller;
        if (c != null) {
          _params.onLoadStop?.call(c, WebUri(url));
        }
      },
      onLoadError: (url, code, message) {
        final c = _controller;
        if (c == null) return;
        _params.onReceivedError?.call(
          c,
          WebResourceRequest(url: WebUri(url), method: 'GET'),
          WebResourceError(
            type: WebResourceErrorType.UNKNOWN,
            description: message,
          ),
        );
      },
      shouldOverrideUrlLoading: (url) {
        final c = _controller;
        final handler = _params.shouldOverrideUrlLoading;
        if (c == null || handler == null) return;
        handler(
          c,
          NavigationAction(
            request: URLRequest(url: WebUri(url)),
            isForMainFrame: true,
          ),
        );
      },
      onMessage: (name, payload) async {
        await _controller?.dispatchJavaScriptMessage(name, payload);
      },
      onWebViewCreated: (native) {
        _controller = LinuxWebKitGtkInAppWebViewController(
          LinuxWebKitGtkInAppWebViewControllerCreationParams(
            id: native.viewId,
            webviewParams: _params,
            nativeController: native,
          ),
        );
        final exposed =
            _params.controllerFromPlatform?.call(_controller!) ?? _controller!;
        _params.onWebViewCreated?.call(exposed);
      },
    );
  }

  @override
  void dispose() {
    final isKeepAlive = _params.keepAlive != null;
    _controller?.dispose(isKeepAlive: isKeepAlive);
    _controller = null;
    _params.findInteractionController?.dispose(isKeepAlive: isKeepAlive);
  }

  @override
  T controllerFromPlatform<T>(PlatformInAppWebViewController controller) {
    throw UnimplementedError();
  }
}
