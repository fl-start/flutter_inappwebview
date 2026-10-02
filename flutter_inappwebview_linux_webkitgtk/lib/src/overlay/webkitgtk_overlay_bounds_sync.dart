import 'dart:async';

import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_geometry.dart';
import 'webkitgtk_geometry_coordinator.dart';
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
  Size? lastLayoutConstraints;
  Size? pendingMeasurementSize;
  Offset? pendingMeasurementOrigin;
  Offset? lastKnownPlaceholderOrigin;
  Size? lastKnownPlaceholderSize;
  int stableMeasurementFrames = 0;
  WebKitGtkOverlayGeometry? lastSentGeometry;
  WebKitGtkOverlayGeometry? lastAckedGeometry;
  int overlayGeneration = 0;

  Timer? _hostResizeDebounce;
  Size? _lastNotifiedHostSize;

  bool computeShouldBeVisible();
  Future<void> setNativeVisibility(bool visible);

  void claimOverlayGeneration() {
    overlayGeneration = WebKitGtkGeometrySequence.nextGeneration();
  }

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
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  void resyncIfPlaceholderMoved() {
    final measured = _measureSlot();
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

  Rect? _measureSlot() {
    final hostBounds =
        WebKitGtkOverlayHooks.boundsProvider?.call(overlayViewId);
    if (hostBounds != null) return hostBounds;

    final placeholderContext = overlayPlaceholderKey.currentContext;
    if (placeholderContext == null) return null;
    final placeholderBox =
        placeholderContext.findRenderObject() as RenderBox?;
    final renderView = renderViewForContext(placeholderContext) ??
        (RendererBinding.instance.renderViews.isEmpty
            ? null
            : RendererBinding.instance.renderViews.first);
    if (placeholderBox == null ||
        !placeholderBox.hasSize ||
        renderView == null) {
      return null;
    }
    return measureFlViewVisibleRect(
      placeholder: placeholderBox,
      renderView: renderView,
    );
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

  void _scheduleHostResizeHook(Size size) {
    final last = _lastNotifiedHostSize;
    if (last != null &&
        (last.width - size.width).abs() < 0.5 &&
        (last.height - size.height).abs() < 0.5) {
      return;
    }
    _lastNotifiedHostSize = size;
    _hostResizeDebounce?.cancel();
    _hostResizeDebounce = Timer(const Duration(milliseconds: 250), () {
      unawaited(notifyPageHostResized());
    });
  }

  void disposeBoundsSync() {
    _hostResizeDebounce?.cancel();
    _hostResizeDebounce = null;
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

    final renderView = () {
      final placeholderContext = overlayPlaceholderKey.currentContext;
      if (placeholderContext != null) {
        return renderViewForContext(placeholderContext);
      }
      return RendererBinding.instance.renderViews.isEmpty
          ? null
          : RendererBinding.instance.renderViews.first;
    }();
    if (renderView == null) return;

    try {
      final measured = _measureSlot();
      if (measured == null) return;

      final occluded = applyOverlayOccluders(
        measured,
        WebKitGtkOverlayOccluders.rects,
      );
      if (occluded.isEmpty || occluded.width < 80 || occluded.height < 80) {
        if (overlayNativeVisible) {
          overlayNativeVisible = false;
          unawaited(setNativeVisibility(false));
        }
        return;
      }

      final overlayOffset = occluded.topLeft;
      final size = occluded.size;
      final force = bypassDebounce ||
          WebKitGtkOverlayHooks.forceImmediateBoundsSync ||
          WebKitGtkOverlayHooks.boundsProvider?.call(overlayViewId) != null;

      if (!force) {
        final measurement = size;
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
          scheduleNativeBoundsSync();
          return;
        }
      }

      final dpr = renderView.flutterView.devicePixelRatio;
      final logicalViewSize = renderView.size;
      final geometry = WebKitGtkOverlayGeometry(
        viewId: overlayViewId,
        sequence: WebKitGtkGeometrySequence.next(),
        generation: overlayGeneration,
        visible: overlayNativeVisible,
        left: overlayOffset.dx,
        top: overlayOffset.dy,
        right: overlayOffset.dx + size.width,
        bottom: overlayOffset.dy + size.height,
        viewWidth: logicalViewSize.width,
        viewHeight: logicalViewSize.height,
        devicePixelRatio: dpr,
      );
      final baseline = lastAckedGeometry ?? lastSentGeometry;
      if (!force &&
          baseline != null &&
          baseline.nearlyEquals(geometry)) {
        return;
      }

      lastSentGeometry = geometry;
      maybeClaimActiveReader();
      WebKitGtkOverlayHooks.onNativeBoundsSent?.call(
        x: geometry.left,
        y: geometry.top,
        width: geometry.width,
        height: geometry.height,
      );
      wkzTrace(
        overlayViewId,
        'setBounds seq=${geometry.sequence} gen=${geometry.generation} '
        'x=${geometry.left.round()},y=${geometry.top.round()} '
        'size=${geometry.width.round()}x${geometry.height.round()} '
        'view=${geometry.viewWidth.round()}x${geometry.viewHeight.round()} '
        'dpr=${geometry.devicePixelRatio.toStringAsFixed(2)} '
        'space=${WebKitGtkOverlayGeometry.coordinateSpace} '
        'visible=$overlayNativeVisible',
      );
      unawaited(_sendGeometry(geometry));
      if (overlayNativeVisible) {
        unawaited(setNativeVisibility(true));
      }
    } catch (e, st) {
      if (kWebKitZOrderTrace) {
        debugPrint('🐧[WKZ v$overlayViewId] COORD sync failed: $e\n$st');
      }
    }
  }

  Future<void> _sendGeometry(WebKitGtkOverlayGeometry target) async {
    try {
      final raw = await WebKitGtkChannelDispatcher.channel.invokeMethod(
        'setBounds',
        target.toMethodChannelArgs(),
      );
      var applied = true;
      if (raw is Map && raw['applied'] == false) {
        applied = false;
      }
      if (!mounted) return;
      if (applied) {
        lastAckedGeometry = target;
        _scheduleHostResizeHook(Size(target.width, target.height));
      }
      if (raw is Map) {
        WebKitGtkOverlayHooks.onNativeBoundsAck?.call(
          applied: applied,
          sequence: (raw['seq'] as num?)?.toInt() ?? target.sequence,
          x: (raw['x'] as num?)?.toDouble() ?? target.left,
          y: (raw['y'] as num?)?.toDouble() ?? target.top,
          width: (raw['width'] as num?)?.toDouble() ?? target.width,
          height: (raw['height'] as num?)?.toDouble() ?? target.height,
        );
      }
    } catch (_) {}
  }

  void resetBoundsMeasurement({required bool forceImmediate}) {
    lastSentGeometry = null;
    lastAckedGeometry = null;
    pendingMeasurementSize = null;
    pendingMeasurementOrigin = null;
    stableMeasurementFrames = 0;
    if (forceImmediate) {
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
    }
  }
}
