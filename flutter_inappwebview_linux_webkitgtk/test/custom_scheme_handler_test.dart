import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/src/overlay/webkitgtk_custom_scheme.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';
import 'package:flutter_test/flutter_test.dart';
import 'dart:typed_data';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('parseCustomSchemeMethodCall', () {
    test('parses Windows/iOS-shaped appmsg request args', () {
      final request = parseCustomSchemeMethodCall(
        const MethodCall('onLoadResourceWithCustomScheme', {
          'viewId': 1,
          'request': {
            'url': 'appmsg://local/m/token/body/id1',
            'method': 'GET',
            'headers': {'Accept': 'text/html'},
            'isForMainFrame': true,
          },
        }),
      );

      expect(request, isNotNull);
      expect(request!.url.scheme, 'appmsg');
      expect(request.url.host, 'local');
      expect(request.url.path, '/m/token/body/id1');
      expect(request.method, 'GET');
      expect(request.isForMainFrame, isTrue);
      expect(request.headers?['Accept'], 'text/html');
    });

    test('returns null for unknown methods', () {
      expect(
        parseCustomSchemeMethodCall(const MethodCall('onLoadStart', {})),
        isNull,
      );
    });

    test('CustomSchemeResponse wire format includes raw bytes', () {
      final response = CustomSchemeResponse(
        data: Uint8List.fromList([1, 2, 3]),
        contentType: 'application/octet-stream',
        contentEncoding: 'utf-8',
      );
      final map = response.toMap();
      expect(map['data'], isA<Uint8List>());
      expect(map['contentType'], 'application/octet-stream');
      expect(map['contentEncoding'], 'utf-8');
    });
  });
}
