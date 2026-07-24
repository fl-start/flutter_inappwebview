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

    // Broadcast load/message events that omit viewId to every view handler,
    // then the legacy fallback.
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
}
