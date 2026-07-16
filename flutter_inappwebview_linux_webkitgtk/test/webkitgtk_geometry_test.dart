import 'package:flutter/material.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/flutter_inappwebview_linux_webkitgtk.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('WebKitGtkOverlayGeometry', () {
    test('roundEdges rounds edges then derives size', () {
      const geo = WebKitGtkOverlayGeometry(
        viewId: 1,
        sequence: 1,
        visible: true,
        left: 10.4,
        top: 20.6,
        right: 110.4,
        bottom: 220.6,
        viewWidth: 800,
        viewHeight: 600,
        devicePixelRatio: 1.25,
      );
      final rounded = geo.roundEdges();
      expect(rounded.left, 10);
      expect(rounded.top, 21);
      expect(rounded.right, 110);
      expect(rounded.bottom, 221);
      expect(rounded.width, 100);
      expect(rounded.height, 200);
    });

    test('nearlyEquals tolerates sub-pixel noise', () {
      const a = WebKitGtkOverlayGeometry(
        viewId: 1,
        sequence: 1,
        visible: true,
        left: 10,
        top: 20,
        right: 110,
        bottom: 220,
        viewWidth: 800,
        viewHeight: 600,
        devicePixelRatio: 1.0,
      );
      const b = WebKitGtkOverlayGeometry(
        viewId: 1,
        sequence: 2,
        visible: true,
        left: 10.2,
        top: 20.1,
        right: 110.3,
        bottom: 220.2,
        viewWidth: 800,
        viewHeight: 600,
        devicePixelRatio: 1.0,
      );
      expect(a.nearlyEquals(b), isTrue);
    });

    test('toMethodChannelArgs includes sequence and coordinateSpace', () {
      const geo = WebKitGtkOverlayGeometry(
        viewId: 7,
        sequence: 42,
        visible: true,
        left: 1.2,
        top: 3.4,
        right: 101.2,
        bottom: 203.4,
        viewWidth: 1280,
        viewHeight: 800,
        devicePixelRatio: 1.5,
      );
      final args = geo.toMethodChannelArgs();
      expect(args['viewId'], 7);
      expect(args['sequence'], 42);
      expect(args['coordinateSpace'], 'flutterLogical');
      expect(args['visible'], isTrue);
      expect(args['left'], isA<double>());
      expect(args['devicePixelRatio'], 1.5);
    });

    test('roundRectEdges avoids independent dimension drift', () {
      final rounded = roundRectEdges(
        const Rect.fromLTWH(10.6, 20.4, 100.3, 50.7),
      );
      expect(rounded.left, 11);
      expect(rounded.top, 20);
      expect(rounded.right, 111); // 10.6+100.3=110.9 → 111
      expect(rounded.bottom, 71); // 20.4+50.7=71.1 → 71
      expect(rounded.width, 100);
      expect(rounded.height, 51);
    });
  });

  group('WebKitGtkOverlayHooks boundsProvider contract', () {
    tearDown(() {
      WebKitGtkOverlayHooks.boundsProvider = null;
      WebKitGtkOverlayHooks.activeEmbeddedViewId = null;
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
      WebKitGtkOverlayHooks.rootPopupCount.value = 0;
    });

    test('per-view provider can isolate reader from composer', () {
      WebKitGtkOverlayHooks.activeEmbeddedViewId = 1;
      WebKitGtkOverlayHooks.boundsProvider = (viewId) {
        if (viewId != 1) return null;
        return const Rect.fromLTWH(100, 80, 900, 600);
      };

      expect(
        WebKitGtkOverlayHooks.boundsProvider!(1),
        const Rect.fromLTWH(100, 80, 900, 600),
      );
      expect(WebKitGtkOverlayHooks.boundsProvider!(2), isNull);
    });

    test('stale sequence values remain monotonic via geometry', () {
      const older = WebKitGtkOverlayGeometry(
        viewId: 1,
        sequence: 5,
        visible: true,
        left: 0,
        top: 0,
        right: 10,
        bottom: 10,
        viewWidth: 100,
        viewHeight: 100,
        devicePixelRatio: 1,
      );
      const newer = WebKitGtkOverlayGeometry(
        viewId: 1,
        sequence: 6,
        visible: true,
        left: 1,
        top: 1,
        right: 11,
        bottom: 11,
        viewWidth: 100,
        viewHeight: 100,
        devicePixelRatio: 1,
      );
      expect(newer.sequence > older.sequence, isTrue);
      expect(older.nearlyEquals(newer), isFalse);
    });
  });
}
