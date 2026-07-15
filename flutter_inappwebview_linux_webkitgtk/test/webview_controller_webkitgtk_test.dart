import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/src/overlay/webview_controller_webkitgtk.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const channel = MethodChannel('webview_webkitgtk');
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
  final calls = <MethodCall>[];

  setUp(() {
    calls.clear();
    messenger.setMockMethodCallHandler(channel, (call) async {
      calls.add(call);
      return switch (call.method) {
        'canGoBack' => true,
        'getCurrentUrl' => 'https://example.test/',
        'evaluateJavaScript' => '42',
        _ => null,
      };
    });
  });

  tearDown(() {
    messenger.setMockMethodCallHandler(channel, null);
  });

  test('loadUrl forwards the view id, URL, and headers', () async {
    final controller = WebViewControllerWebKitGTK(7);

    await controller.loadUrl(
      'https://example.test/',
      headers: const {'Authorization': 'Bearer token'},
    );

    expect(calls, hasLength(1));
    expect(calls.single.method, 'loadUrl');
    expect(calls.single.arguments, {
      'viewId': 7,
      'url': 'https://example.test/',
      'headers': {'Authorization': 'Bearer token'},
    });
  });

  test('controller queries return native channel values', () async {
    final controller = WebViewControllerWebKitGTK(9);

    expect(await controller.canGoBack(), isTrue);
    expect(await controller.getCurrentUrl(), 'https://example.test/');
    expect(await controller.evaluateJavaScript('6 * 7'), '42');
    expect(calls.map((call) => call.method), [
      'canGoBack',
      'getCurrentUrl',
      'evaluateJavaScript',
    ]);
  });

  test('canGoBack defaults to false when native returns null', () async {
    messenger.setMockMethodCallHandler(channel, (_) async => null);

    expect(await WebViewControllerWebKitGTK(11).canGoBack(), isFalse);
  });
}
