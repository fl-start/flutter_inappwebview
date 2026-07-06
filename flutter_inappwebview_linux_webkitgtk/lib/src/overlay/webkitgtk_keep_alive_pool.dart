import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import 'webview_controller_webkitgtk.dart';

/// Preserves native WebKitGTK overlay instances across widget disposal.
class WebKitGtkKeepAlivePool {
  WebKitGtkKeepAlivePool._();

  static final Map<String, _Entry> _entries = {};

  static int? viewIdFor(String? keepAliveId) {
    if (keepAliveId == null) return null;
    return _entries[keepAliveId]?.viewId;
  }

  static WebViewControllerWebKitGTK? controllerFor(String? keepAliveId) {
    if (keepAliveId == null) return null;
    return _entries[keepAliveId]?.controller;
  }

  static void store({
    required String keepAliveId,
    required int viewId,
    required WebViewControllerWebKitGTK controller,
  }) {
    _entries[keepAliveId] = _Entry(viewId: viewId, controller: controller);
  }

  static void release(String keepAliveId) {
    _entries.remove(keepAliveId);
  }
}

class _Entry {
  _Entry({required this.viewId, required this.controller});
  final int viewId;
  final WebViewControllerWebKitGTK controller;
}
