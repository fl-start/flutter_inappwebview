import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/flutter_inappwebview_linux_webkitgtk.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';
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
      return null;
    });
  });

  tearDown(() {
    messenger.setMockMethodCallHandler(channel, null);
  });

  testWidgets('initial settings are forwarded during native creation', (
    tester,
  ) async {
    final platformWidget = LinuxWebKitGtkInAppWebViewWidget(
      PlatformInAppWebViewWidgetCreationParams(
        initialSettings: InAppWebViewSettings(
          javaScriptEnabled: false,
          incognito: true,
        ),
      ),
    );

    await tester.pumpWidget(
      MaterialApp(
        home: Builder(builder: platformWidget.build),
      ),
    );
    await tester.pump();

    final create = calls.firstWhere((call) => call.method == 'create');
    final arguments = (create.arguments as Map<Object?, Object?>);
    final settings = arguments['settings'] as Map<Object?, Object?>;
    expect(settings['javaScriptEnabled'], isFalse);
    expect(settings['incognito'], isTrue);

    await tester.pumpWidget(const SizedBox.shrink());
  });

  testWidgets('navigation policy is returned to the native caller', (
    tester,
  ) async {
    final platformWidget = LinuxWebKitGtkInAppWebViewWidget(
      PlatformInAppWebViewWidgetCreationParams(
        shouldOverrideUrlLoading: (_, _) async => NavigationActionPolicy.CANCEL,
      ),
    );
    await tester.pumpWidget(
      MaterialApp(home: Builder(builder: platformWidget.build)),
    );
    await tester.pump();

    final reply = Completer<ByteData?>();
    await messenger.handlePlatformMessage(
      channel.name,
      const StandardMethodCodec().encodeMethodCall(
        const MethodCall('shouldOverrideUrlLoading', {
          'url': 'https://example.test/blocked',
        }),
      ),
      reply.complete,
    );

    expect(
      const StandardMethodCodec().decodeEnvelope((await reply.future)!),
      NavigationActionPolicy.CANCEL.toNativeValue(),
    );
    await tester.pumpWidget(const SizedBox.shrink());
  });
}
