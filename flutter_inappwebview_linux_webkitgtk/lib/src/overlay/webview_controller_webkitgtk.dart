import 'package:flutter/services.dart';

class WebViewControllerWebKitGTK {
  static const MethodChannel _channel = MethodChannel('webview_webkitgtk');

  final int _viewId;

  WebViewControllerWebKitGTK(this._viewId);

  Future<void> loadUrl(String url, {Map<String, String>? headers}) async {
    await _channel.invokeMethod('loadUrl', {
      'viewId': _viewId,
      'url': url,
      if (headers != null) 'headers': headers,
    });
  }

  Future<void> loadHtml(String html, {String? baseUrl}) async {
    await _channel.invokeMethod('loadHtml', {
      'viewId': _viewId,
      'html': html,
      if (baseUrl != null) 'baseUrl': baseUrl,
    });
  }

  Future<String?> evaluateJavaScript(String js) async {
    return _channel.invokeMethod<String>('evaluateJavaScript', {
      'viewId': _viewId,
      'javascript': js,
    });
  }

  Future<void> reload() async {
    await _channel.invokeMethod('reload', {'viewId': _viewId});
  }

  Future<void> goBack() async {
    await _channel.invokeMethod('goBack', {'viewId': _viewId});
  }

  Future<bool> canGoBack() async {
    final result = await _channel.invokeMethod<bool>('canGoBack', {
      'viewId': _viewId,
    });
    return result ?? false;
  }

  Future<void> addJavaScriptChannel(String channelName) async {
    await _channel.invokeMethod('addJavaScriptChannel', {
      'viewId': _viewId,
      'channelName': channelName,
    });
  }

  Future<String?> getCurrentUrl() async {
    return _channel.invokeMethod<String>('getCurrentUrl', {
      'viewId': _viewId,
    });
  }

  int get viewId => _viewId;
}
