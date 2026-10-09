import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/src/overlay/webkitgtk_channel_dispatcher.dart';

/// Delivers [method] to the dispatcher as if native WebKitGTK sent it.
Future<dynamic> nativeCall(String method, Object? arguments) async {
  const codec = StandardMethodCodec();
  final reply = Completer<ByteData?>();
  await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
      .handlePlatformMessage(
    WebKitGtkChannelDispatcher.channel.name,
    codec.encodeMethodCall(MethodCall(method, arguments)),
    reply.complete,
  );
  return codec.decodeEnvelope((await reply.future)!);
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const reader = 1;
  const composer = 2;
  final received = <int, List<MethodCall>>{};

  setUp(() {
    received.clear();
    for (final id in [reader, composer]) {
      WebKitGtkChannelDispatcher.registerView(id, (call) async {
        received.putIfAbsent(id, () => []).add(call);
        return call.method == 'shouldOverrideUrlLoading' ? 1 : 'from $id';
      });
    }
  });

  tearDown(() {
    WebKitGtkChannelDispatcher.unregisterView(reader);
    WebKitGtkChannelDispatcher.unregisterView(composer);
  });

  test('onMessage reaches the view that sent it, not the newest view',
      () async {
    final result = await nativeCall('onMessage', {
      'viewId': reader,
      'name': 'emailComposer',
      'payload': '{"id":"1","args":[]}',
    });

    expect(result, 'from $reader');
    expect(received[reader], hasLength(1));
    expect(received[composer], isNull);
  });

  test('navigation from a view without a handler is cancelled', () async {
    final result = await nativeCall('shouldOverrideUrlLoading', {
      'viewId': 99,
      'url': 'https://example.com/',
    });

    expect(result, 0);
    expect(received, isEmpty);
  });

  test('message from a view without a handler rejects its promise', () async {
    final result = await nativeCall('onMessage', {
      'viewId': 99,
      'name': 'emailComposer',
      'payload': '{"id":"7","args":["x"]}',
    });

    final reply = jsonDecode(result as String) as Map;
    expect(reply['id'], '7');
    expect(reply['error'], contains('99'));
    expect(received, isEmpty);
  });

  test('load events from a view without a handler are dropped', () async {
    final result = await nativeCall('onLoadStop', {
      'viewId': 99,
      'url': 'appmsg://local/shell.html',
    });

    expect(result, isNull);
    expect(received, isEmpty);
  });
}
