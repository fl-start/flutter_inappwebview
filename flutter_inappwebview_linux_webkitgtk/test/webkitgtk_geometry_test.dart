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
      expect(args['generation'], 0);
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

  group('overlayViewSizeIsStale', () {
    test('steady state V equals A is not stale', () {
      expect(
        overlayViewSizeIsStale(
          flutterViewW: 1000,
          flutterViewH: 800,
          flViewAllocW: 1000,
          flViewAllocH: 800,
        ),
        isFalse,
      );
    });

    test('maximize lag is stale', () {
      expect(
        overlayViewSizeIsStale(
          flutterViewW: 1280,
          flutterViewH: 800,
          flViewAllocW: 2560,
          flViewAllocH: 800,
        ),
        isTrue,
      );
    });

    test('invalid logical size is not treated as stale', () {
      expect(
        overlayViewSizeIsStale(
          flutterViewW: 0,
          flutterViewH: 800,
          flViewAllocW: 2000,
          flViewAllocH: 800,
        ),
        isFalse,
      );
    });
  });

  group('overlayIntersectHost', () {
    test('clips overflow on shrink', () {
      final hit = overlayIntersectHost(
        x: 100,
        y: 50,
        w: 900,
        h: 700,
        hostW: 800,
        hostH: 600,
      );
      expect(hit.nonEmpty, isTrue);
      expect(hit.x, 100);
      expect(hit.y, 50);
      expect(hit.w, 700);
      expect(hit.h, 550);
    });

    test('empty when slot is off-host', () {
      final hit = overlayIntersectHost(
        x: 900,
        y: 10,
        w: 200,
        h: 200,
        hostW: 800,
        hostH: 600,
      );
      expect(hit.nonEmpty, isFalse);
    });
  });

  group('WebKitGtkGeometrySequence', () {
    setUp(WebKitGtkGeometrySequence.resetForTest);

    test('is process-global and monotonic', () {
      expect(WebKitGtkGeometrySequence.next(), 1);
      expect(WebKitGtkGeometrySequence.next(), 2);
      expect(WebKitGtkGeometrySequence.nextGeneration(), 1);
      expect(WebKitGtkGeometrySequence.next(), 3);
    });
  });

  group('applyOverlayOccluders', () {
    test('shrinks a bottom strip', () {
      final slot = const Rect.fromLTWH(100, 80, 400, 500);
      final toast = const Rect.fromLTWH(100, 520, 400, 60);
      final result = applyOverlayOccluders(slot, [toast]);
      expect(result.top, 80);
      expect(result.bottom, 520);
      expect(result.width, 400);
    });

    test('hides on messy overlap', () {
      final slot = const Rect.fromLTWH(0, 0, 400, 400);
      final card = const Rect.fromLTWH(50, 50, 300, 300);
      expect(applyOverlayOccluders(slot, [card]), Rect.zero);
    });
  });

  group('WebKitGtkOverlayHooks boundsProvider contract', () {
    tearDown(() {
      WebKitGtkOverlayHooks.boundsProvider = null;
      WebKitGtkOverlayHooks.activeEmbeddedViewId = null;
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
      WebKitGtkOverlayHooks.rootPopupCount.value = 0;
      WebKitGtkOverlayHooks.exclusiveShellViewId.value = null;
    });

    test('exclusiveShellViewId setter is idempotent', () {
      WebKitGtkOverlayHooks.setExclusiveShellViewId(7);
      expect(WebKitGtkOverlayHooks.exclusiveShellViewId.value, 7);
      WebKitGtkOverlayHooks.setExclusiveShellViewId(7);
      expect(WebKitGtkOverlayHooks.exclusiveShellViewId.value, 7);
      WebKitGtkOverlayHooks.setExclusiveShellViewId(null);
      expect(WebKitGtkOverlayHooks.exclusiveShellViewId.value, isNull);
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

  group('measureFlViewLogicalRect', () {
    testWidgets('returns placeholder size in logical pixels', (tester) async {
      final key = GlobalKey();
      await tester.pumpWidget(
        MaterialApp(
          home: Align(
            alignment: Alignment.topLeft,
            child: SizedBox(
              key: key,
              width: 200,
              height: 100,
              child: const ColoredBox(color: Color(0xFF000000)),
            ),
          ),
        ),
      );
      final box = key.currentContext!.findRenderObject()! as RenderBox;
      final renderView = tester.binding.renderViews.first;
      final rect = measureFlViewLogicalRect(
        placeholder: box,
        renderView: renderView,
      );
      expect(rect, isNotNull);
      expect(rect!.width, closeTo(200, 0.5));
      expect(rect.height, closeTo(100, 0.5));
    });

    testWidgets('intersects an ancestor clip', (tester) async {
      final key = GlobalKey();
      await tester.pumpWidget(
        MaterialApp(
          home: Align(
            alignment: Alignment.topLeft,
            child: SizedBox(
              width: 100,
              height: 80,
              child: ClipRect(
                child: OverflowBox(
                  maxWidth: 200,
                  maxHeight: 160,
                  child: SizedBox(
                    key: key,
                    width: 200,
                    height: 160,
                    child: const ColoredBox(color: Color(0xFF000000)),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      final box = key.currentContext!.findRenderObject()! as RenderBox;
      final renderView = tester.binding.renderViews.first;
      final rect = measureFlViewVisibleRect(
        placeholder: box,
        renderView: renderView,
      );
      expect(rect, isNotNull);
      expect(rect!.width, lessThanOrEqualTo(100.5));
      expect(rect.height, lessThanOrEqualTo(80.5));
    });
  });

  group('WebKitGtkGeometryCoordinator', () {
    tearDown(() {
      WebKitGtkOverlayOccluders.clearForTest();
    });

    testWidgets('flush calls each registered client once', (tester) async {
      var count = 0;
      void client() => count++;
      WebKitGtkGeometryCoordinator.instance.register(client);
      addTearDown(
        () => WebKitGtkGeometryCoordinator.instance.unregister(client),
      );
      WebKitGtkGeometryCoordinator.instance.schedule();
      WebKitGtkGeometryCoordinator.instance.schedule();
      await tester.pump();
      expect(count, 1);
    });
  });
}
