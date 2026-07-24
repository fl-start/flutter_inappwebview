import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

/// Optional process-wide resolver for `appmsg://` (and other custom schemes).
///
/// Overlay widgets normally forward scheme requests through
/// [PlatformInAppWebViewWidgetCreationParams.onLoadResourceWithCustomScheme],
/// which requires a live controller. That path races Flutter rebuilds on Linux
/// GtkOverlay. Registering [globalResolver] lets the host app serve content
/// without a controller (e.g. secMail LocalhostServerService).
typedef WebKitGtkCustomSchemeResolver = Future<Map<String, dynamic>?> Function(
  WebResourceRequest request,
);

/// Linux WebKitGTK custom-scheme helpers.
abstract final class WebKitGtkCustomScheme {
  /// Host-app content resolver; consulted when the per-view handler returns null
  /// or an empty payload.
  static WebKitGtkCustomSchemeResolver? globalResolver;
}

/// Parses a native `onLoadResourceWithCustomScheme` [MethodCall] into a
/// [WebResourceRequest] (Windows/iOS-compatible `{request: {...}}` shape).
WebResourceRequest? parseCustomSchemeMethodCall(MethodCall call) {
  if (call.method != 'onLoadResourceWithCustomScheme') return null;
  final args = call.arguments;
  if (args is! Map) return null;
  final requestMap = args['request'];
  if (requestMap is! Map) return null;
  final asMap = Map<String, dynamic>.from(
    requestMap.map((key, value) => MapEntry(key.toString(), value)),
  );
  final headersRaw = asMap['headers'];
  if (headersRaw is Map) {
    asMap['headers'] = Map<String, String>.from(
      headersRaw.map(
        (key, value) => MapEntry(key.toString(), value.toString()),
      ),
    );
  }
  return WebResourceRequest.fromMap(asMap);
}

bool _schemeResponseHasPayload(dynamic result) {
  if (result is! Map) return false;
  final data = result['data'];
  if (data is List) return data.isNotEmpty;
  return false;
}

/// Resolves a custom-scheme [MethodCall] via [globalResolver] then [viewHandler].
///
/// Prefers [globalResolver] so Linux GtkOverlay rebuild races (null controller)
/// cannot block `appmsg://` content.
Future<dynamic> resolveCustomSchemeMethodCall(
  MethodCall call, {
  Future<dynamic> Function(WebResourceRequest request)? viewHandler,
}) async {
  final request = parseCustomSchemeMethodCall(call);
  if (request == null) return null;

  final global = WebKitGtkCustomScheme.globalResolver;
  if (global != null) {
    final fromGlobal = await global(request);
    if (_schemeResponseHasPayload(fromGlobal)) return fromGlobal;
  }

  if (viewHandler != null) {
    final fromView = await viewHandler(request);
    if (_schemeResponseHasPayload(fromView)) return fromView;
  }

  return null;
}
