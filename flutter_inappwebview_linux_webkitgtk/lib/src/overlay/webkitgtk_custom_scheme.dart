import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

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
