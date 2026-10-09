import 'dart:convert';

import 'package:flutter/services.dart';

import 'webkitgtk_custom_scheme.dart';

/// Single owner of the `webview_webkitgtk` [MethodChannel] handler.
///
/// Multiple overlay widgets and legacy host services must not call
/// [MethodChannel.setMethodCallHandler] directly — the last writer wins and
/// silently drops `onHostLayoutChanged` / load callbacks.
class WebKitGtkChannelDispatcher {
  WebKitGtkChannelDispatcher._();

  static const MethodChannel channel = MethodChannel('webview_webkitgtk');

  static final Map<int, Future<dynamic> Function(MethodCall)> _viewHandlers =
      <int, Future<dynamic> Function(MethodCall)>{};

  static Future<dynamic> Function(MethodCall)? _fallbackHandler;
  static bool _installed = false;

  /// Events native tags with the sending view's id. When that view has no
  /// handler (disposed, or parked in the keep-alive pool) no other view may
  /// answer: a peer would apply its own navigation policy and JS handlers.
  static const Set<String> _viewScopedMethods = {
    'onLoadStart',
    'onLoadStop',
    'onLoadError',
    'shouldOverrideUrlLoading',
    'onMessage',
  };

  /// [NavigationActionPolicy.CANCEL] on the wire.
  static const int _navigationCancel = 0;

  static void ensureInstalled() {
    if (_installed) return;
    _installed = true;
    channel.setMethodCallHandler(_dispatch);
  }

  static void registerView(
    int viewId,
    Future<dynamic> Function(MethodCall) handler,
  ) {
    ensureInstalled();
    _viewHandlers[viewId] = handler;
  }

  static void unregisterView(int viewId) {
    _viewHandlers.remove(viewId);
  }

  /// Legacy / host-app callbacks (separate-window path, shortcuts, etc.).
  static void setFallbackHandler(
    Future<dynamic> Function(MethodCall)? handler,
  ) {
    ensureInstalled();
    _fallbackHandler = handler;
  }

  static int? _viewIdFrom(MethodCall call) {
    final args = call.arguments;
    if (args is Map) {
      final raw = args['viewId'];
      if (raw is int) return raw;
      if (raw is num) return raw.toInt();
    }
    return null;
  }

  static Future<dynamic> _dispatch(MethodCall call) async {
    final viewId = _viewIdFrom(call);

    // Host layout changes must reach every embedded overlay — do not rely on
    // a single last-registered handler.
    if (call.method == 'onHostLayoutChanged') {
      if (viewId != null) {
        final targeted = _viewHandlers[viewId];
        if (targeted != null) return targeted(call);
      }
      for (final handler in _viewHandlers.values.toList(growable: false)) {
        await handler(call);
      }
      return null;
    }

    if (viewId != null) {
      final targeted = _viewHandlers[viewId];
      if (targeted != null) {
        final result = await targeted(call);
        // Custom-scheme content is view-agnostic (Dart LocalhostServerService).
        // If this view's overlay returned null (handler not wired yet), try peers
        // before falling through to native scheme_routes / NOT_FOUND.
        if (result != null ||
            call.method != 'onLoadResourceWithCustomScheme') {
          return result;
        }
      }
    }

    if (viewId != null && _viewScopedMethods.contains(call.method)) {
      return _unroutedViewEvent(call, viewId);
    }

    if (call.method == 'onLoadResourceWithCustomScheme') {
      for (final entry in _viewHandlers.entries) {
        if (entry.key == viewId) continue;
        final result = await entry.value(call);
        if (result != null) return result;
      }
      // Process-wide resolver (no controller required).
      final global = await resolveCustomSchemeMethodCall(call);
      if (global != null) return global;
    }

    // Events that omit viewId (separate-window callbacks, older native
    // builds) go to the only view, then the legacy fallback.
    if (_viewHandlers.length == 1) {
      return _viewHandlers.values.first(call);
    }

    final fallback = _fallbackHandler;
    if (fallback != null) return fallback(call);

    if (_viewHandlers.isNotEmpty) {
      // Prefer the highest viewId (newest reader) when multiple exist.
      final newestId = _viewHandlers.keys.reduce((a, b) => a > b ? a : b);
      return _viewHandlers[newestId]!(call);
    }

    return null;
  }

  /// Fail-closed answer for a view-scoped event whose view has no handler.
  static dynamic _unroutedViewEvent(MethodCall call, int viewId) {
    switch (call.method) {
      case 'shouldOverrideUrlLoading':
        // Local mail documents must still commit when the view handler is
        // missing (keep-alive gap, dispose race). Cancelling them leaves the
        // reader on about:blank. Remote URLs stay cancelled.
        if (_isLocalMailNavigation(_urlFrom(call))) return 1;
        return _navigationCancel;
      case 'onMessage':
        // Reject with the message id so the page's callHandler promise
        // settles instead of waiting forever.
        return jsonEncode({
          'id': _bridgeMessageId(call.arguments),
          'error': 'No JavaScript handler for view $viewId',
        });
      default:
        return null;
    }
  }

  static String? _urlFrom(MethodCall call) {
    final args = call.arguments;
    if (args is Map) {
      final raw = args['url'];
      if (raw is String) return raw;
    }
    return null;
  }

  /// `appmsg:` mail content and `about:` documents (blank / srcdoc).
  static bool _isLocalMailNavigation(String? url) {
    if (url == null || url.isEmpty) return false;
    final lower = url.toLowerCase();
    return lower.startsWith('appmsg:') || lower.startsWith('about:');
  }

  static dynamic _bridgeMessageId(dynamic arguments) {
    if (arguments is! Map) return null;
    final payload = arguments['payload'];
    if (payload is! String) return null;
    try {
      final decoded = jsonDecode(payload);
      return decoded is Map ? decoded['id'] : null;
    } catch (_) {
      return null;
    }
  }
}
