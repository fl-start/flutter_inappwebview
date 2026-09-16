import 'dart:async';

import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_geometry.dart';
import 'webkitgtk_overlay_hooks.dart';
import 'webview_controller_webkitgtk.dart';

/// Toggle for verbose Linux WebKit z-order tracing in the terminal.
const bool kWebKitZOrderTrace = false;

void wkzTrace(int viewId, String msg) {
  if (!kWebKitZOrderTrace) return;
  debugPrint('🐧[WKZ v$viewId] $msg');
}

/// Coalesce/measure/send GtkOverlay `setBounds`.
mixin WebKitGtkOverlayBoundsSync<T extends StatefulWidget> on State<T> {
  int get overlayViewId;
  WebViewControllerWebKitGTK? get overlayController;
  bool get overlayInitialized;
  GlobalKey get overlayPlaceholderKey;

  bool overlayNativeVisible = false;
  bool boundsSendScheduled = false;
  Size? lastLayoutConstraints;
  Size? pendingMeasurementSize;
  Offset? pendingMeasurementOrigin;
  Offset? lastKnownPlaceholderOrigin;
  Size? lastKnownPlaceholderSize;
  int stableMeasurementFrames = 0;
  int geometrySequence = 0;
  WebKitGtkOverlayGeometry? lastSentGeometry;
  WebKitGtkOverlayGeometry? pendingGeometry;

  bool computeShouldBeVisible();
  Future<void> setNativeVisibility(bool visible);

  void maybeClaimActiveReader() {
    if (!overlayNativeVisible || !mounted) return;
    final route = ModalRoute.of(context);
    final rootNav = Navigator.maybeOf(context, rootNavigator: true);
    final isOnRootNav = route?.navigator == rootNav;
    if (isOnRootNav) return;
    if (WebKitGtkOverlayHooks.exclusiveShellViewId.value != null) {
      return;
    }
    WebKitGtkOverlayHooks.activeEmbeddedViewId = overlayViewId;
  }

  void scheduleNativeBoundsSync({int frames = 1}) {
    void run(int remaining) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) return;
        syncNativeWindowPosition(bypassDebounce: true);
        if (remaining > 1) {
          run(remaining - 1);
        }
      });
    }

    run(frames);
  }

  void resyncIfPlaceholderMoved() {
    final placeholderContext = overlayPlaceholderKey.currentContext;
    if (placeholderContext == null) return;
    final placeholderBox =
        placeholderContext.findRenderObject() as RenderBox?;
    final renderView = renderViewForContext(placeholderContext) ??
        (RendererBinding.instance.renderViews.isEmpty
            ? null
            : RendererBinding.instance.renderViews.first);
    if (placeholderBox == null ||
        !placeholderBox.hasSize ||
        renderView == null) {
      return;
    }
    final measured = measureFlViewLogicalRect(
      placeholder: placeholderBox,
      renderView: renderView,
    );
    if (measured == null) return;
    final origin = measured.topLeft;
    final size = measured.size;
    const epsilon = 0.5;
    final moved = lastKnownPlaceholderOrigin == null ||
        (lastKnownPlaceholderOrigin!.dx - origin.dx).abs() >= epsilon ||
        (lastKnownPlaceholderOrigin!.dy - origin.dy).abs() >= epsilon;
    final resized = lastKnownPlaceholderSize == null ||
        (lastKnownPlaceholderSize!.width - size.width).abs() >= epsilon ||
        (lastKnownPlaceholderSize!.height - size.height).abs() >= epsilon;
    if (!moved && !resized) return;
    lastKnownPlaceholderOrigin = origin;
    lastKnownPlaceholderSize = size;
    lastSentGeometry = null;
    syncNativeWindowPosition(bypassDebounce: true);
  }

  Future<void> notifyPageHostResized() async {
    final controller = overlayController;
    if (controller == null || !overlayInitialized) return;
    try {
      await controller.evaluateJavaScript('''
(function(){
  try{
    if(window.__scommOnHostResize){window.__scommOnHostResize();return 'hook';}
    var root=document.getElementById('scomm-scroll-root');
    if(root){
      var maxL=Math.max(0,(root.scrollWidth||0)-(root.clientWidth||0));
      var maxT=Math.max(0,(root.scrollHeight||0)-(root.clientHeight||0));
      if(root.scrollLeft>maxL)root.scrollLeft=maxL;
      if(root.scrollTop>maxT)root.scrollTop=maxT;
    }
    window.dispatchEvent(new Event('resize'));
    return 'fallback';
  }catch(e){return String(e);}
})()
''');
    } catch (_) {}
  }

  void syncNativeWindowPosition({bool bypassDebounce = false}) {
    if (!overlayInitialized) return;

    final shouldBeVisible = computeShouldBeVisible();
    if (overlayNativeVisible != shouldBeVisible) {
      overlayNativeVisible = shouldBeVisible;
      unawaited(setNativeVisibility(shouldBeVisible));
    }

    if (!shouldBeVisible) {
      return;
    }

    final Rect? hostBounds =
        WebKitGtkOverlayHooks.boundsProvider?.call(overlayViewId);
    final force = bypassDebounce ||
        WebKitGtkOverlayHooks.forceImmediateBoundsSync ||
        hostBounds != null;

    final placeholderContext = overlayPlaceholderKey.currentContext;
    RenderBox? placeholderBox;
    RenderView? renderView;
    if (placeholderContext != null) {
      placeholderBox = placeholderContext.findRenderObject() as RenderBox?;
      renderView = renderViewForContext(placeholderContext);
    }
    renderView ??= RendererBinding.instance.renderViews.isEmpty
        ? null
        : RendererBinding.instance.renderViews.first;
    if (renderView == null) return;

    try {
      final Offset overlayOffset;
      final Size size;
      if (hostBounds != null) {
        overlayOffset = hostBounds.topLeft;
        size = hostBounds.size;
      } else {
        if (placeholderBox == null || !placeholderBox.hasSize) return;
        final measured = measureFlViewLogicalRect(
          placeholder: placeholderBox,
          renderView: renderView,
        );
        if (measured == null) return;
        overlayOffset = measured.topLeft;
        size = measured.size;
      }

      final double effectiveWidth = size.width;
      final double effectiveHeight = size.height;
      if (effectiveWidth <= 0 || effectiveHeight <= 0) return;

      final dpr = renderView.flutterView.devicePixelRatio;
      final logicalViewSize = renderView.size;
      final viewWidth = logicalViewSize.width;
      final viewHeight = logicalViewSize.height;

      if (!force) {
        final measurement = Size(effectiveWidth, effectiveHeight);
        final origin = overlayOffset;
        final sizeStable = pendingMeasurementSize != null &&
            (pendingMeasurementSize!.width - measurement.width).abs() < 0.5 &&
            (pendingMeasurementSize!.height - measurement.height).abs() < 0.5;
        final originStable = pendingMeasurementOrigin != null &&
            (pendingMeasurementOrigin!.dx - origin.dx).abs() < 0.5 &&
            (pendingMeasurementOrigin!.dy - origin.dy).abs() < 0.5;
        if (sizeStable && originStable) {
          stableMeasurementFrames++;
        } else {
          pendingMeasurementSize = measurement;
          pendingMeasurementOrigin = origin;
          stableMeasurementFrames = 0;
        }
        if (stableMeasurementFrames < 1) {
          scheduleNativeBoundsSync(frames: 1);
          return;
        }
      }

      final geometry = WebKitGtkOverlayGeometry(
        viewId: overlayViewId,
        sequence: ++geometrySequence,
        visible: overlayNativeVisible,
        left: overlayOffset.dx,
        top: overlayOffset.dy,
        right: overlayOffset.dx + effectiveWidth,
        bottom: overlayOffset.dy + effectiveHeight,
        viewWidth: viewWidth,
        viewHeight: viewHeight,
        devicePixelRatio: dpr,
      );
      if (!force &&
          lastSentGeometry != null &&
          lastSentGeometry!.nearlyEquals(geometry)) {
        return;
      }

      void sendGeometry(WebKitGtkOverlayGeometry target) {
        if (!mounted) return;
        pendingGeometry = null;
        lastSentGeometry = target;
        maybeClaimActiveReader();
        WebKitGtkOverlayHooks.onNativeBoundsSent?.call(
          x: target.left,
          y: target.top,
          width: target.width,
          height: target.height,
        );
        wkzTrace(
          overlayViewId,
          'setBounds seq=${target.sequence} '
          'x=${target.left.round()},y=${target.top.round()} '
          'size=${target.width.round()}x${target.height.round()} '
          'view=${target.viewWidth.round()}x${target.viewHeight.round()} '
          'dpr=${target.devicePixelRatio.toStringAsFixed(2)} '
          'space=${WebKitGtkOverlayGeometry.coordinateSpace} '
          'visible=$overlayNativeVisible host=${hostBounds != null}',
        );
        WebKitGtkChannelDispatcher.channel.invokeMethod(
          'setBounds',
          target.toMethodChannelArgs(),
        );
        unawaited(notifyPageHostResized());
        if (overlayNativeVisible) {
          unawaited(setNativeVisibility(true));
        } else {
          unawaited(setNativeVisibility(false));
        }
      }

      if (force) {
        pendingGeometry = null;
        sendGeometry(geometry);
      } else {
        pendingGeometry = geometry;
        if (boundsSendScheduled) return;
        boundsSendScheduled = true;
        WidgetsBinding.instance.addPostFrameCallback((_) {
          boundsSendScheduled = false;
          final target = pendingGeometry;
          if (!mounted || target == null) return;
          sendGeometry(target);
        });
      }
    } catch (e, st) {
      if (kWebKitZOrderTrace) {
        debugPrint('🐧[WKZ v$overlayViewId] COORD sync failed: $e\n$st');
      }
    }
  }

  void resetBoundsMeasurement({required bool forceImmediate}) {
    lastSentGeometry = null;
    pendingMeasurementSize = null;
    pendingMeasurementOrigin = null;
    stableMeasurementFrames = 0;
    if (forceImmediate) {
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
    }
  }
}
