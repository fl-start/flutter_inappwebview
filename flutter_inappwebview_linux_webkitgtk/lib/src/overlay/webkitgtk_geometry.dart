import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

/// Process-global monotonic sequence / attach generation for `setBounds`.
///
/// All senders (overlay mixin, host `pushSetBounds`) MUST use [next]. A
/// microsecond timestamp poisons native (`seq > last` for the instance
/// lifetime) and freezes later mixin updates.
abstract final class WebKitGtkGeometrySequence {
  static int _next = 0;
  static int _generation = 0;

  static int next() => ++_next;

  static int get current => _next;

  static int nextGeneration() => ++_generation;

  static int get generation => _generation;

  @visibleForTesting
  static void resetForTest() {
    _next = 0;
    _generation = 0;
  }
}

/// FlView-local logical geometry for GtkOverlay [setBounds].
///
/// Units: Flutter logical pixels relative to the [RenderView] / FlView origin.
/// Do **not** multiply by [devicePixelRatio] before sending — native applies
/// identity scale (`O = o + S`).
@immutable
class WebKitGtkOverlayGeometry {
  const WebKitGtkOverlayGeometry({
    required this.viewId,
    required this.sequence,
    required this.visible,
    required this.left,
    required this.top,
    required this.right,
    required this.bottom,
    required this.viewWidth,
    required this.viewHeight,
    required this.devicePixelRatio,
    this.generation = 0,
  });

  final int viewId;
  final int sequence;
  final int generation;
  final bool visible;
  final double left;
  final double top;
  final double right;
  final double bottom;
  final double viewWidth;
  final double viewHeight;
  final double devicePixelRatio;

  /// Coordinate space label for the native bridge / diagnostics.
  static const String coordinateSpace = 'flutterLogical';

  double get width => right - left;
  double get height => bottom - top;

  Rect get rect => Rect.fromLTRB(left, top, right, bottom);

  /// Round edges then derive size (avoids 1px gaps under fractional scale).
  WebKitGtkOverlayGeometry roundEdges() {
    final l = left.roundToDouble();
    final t = top.roundToDouble();
    final r = right.roundToDouble();
    final b = bottom.roundToDouble();
    return WebKitGtkOverlayGeometry(
      viewId: viewId,
      sequence: sequence,
      generation: generation,
      visible: visible,
      left: l,
      top: t,
      right: r < l ? l : r,
      bottom: b < t ? t : b,
      viewWidth: viewWidth,
      viewHeight: viewHeight,
      devicePixelRatio: devicePixelRatio,
    );
  }

  bool nearlyEquals(WebKitGtkOverlayGeometry other, {double epsilon = 0.5}) {
    return viewId == other.viewId &&
        visible == other.visible &&
        (left - other.left).abs() < epsilon &&
        (top - other.top).abs() < epsilon &&
        (right - other.right).abs() < epsilon &&
        (bottom - other.bottom).abs() < epsilon &&
        (viewWidth - other.viewWidth).abs() < epsilon &&
        (viewHeight - other.viewHeight).abs() < epsilon &&
        (devicePixelRatio - other.devicePixelRatio).abs() < 0.01;
  }

  Map<String, Object?> toMethodChannelArgs() {
    final rounded = roundEdges();
    return <String, Object?>{
      'viewId': rounded.viewId,
      'sequence': rounded.sequence,
      'generation': rounded.generation,
      'visible': rounded.visible,
      'coordinateSpace': coordinateSpace,
      'x': rounded.left,
      'y': rounded.top,
      'width': rounded.width,
      'height': rounded.height,
      'left': rounded.left,
      'top': rounded.top,
      'right': rounded.right,
      'bottom': rounded.bottom,
      'viewWidth': rounded.viewWidth,
      'viewHeight': rounded.viewHeight,
      'devicePixelRatio': rounded.devicePixelRatio,
    };
  }

  @override
  String toString() =>
      'WebKitGtkOverlayGeometry(v$viewId seq=$sequence gen=$generation '
      'visible=$visible '
      'LTRB=${left.toStringAsFixed(1)},${top.toStringAsFixed(1)},'
      '${right.toStringAsFixed(1)},${bottom.toStringAsFixed(1)} '
      'view=${viewWidth.toStringAsFixed(0)}x${viewHeight.toStringAsFixed(0)} '
      'dpr=${devicePixelRatio.toStringAsFixed(2)})';
}

/// Native `webview_overlay_view_size_is_stale` counterpart. Keep in lockstep
/// with `linux/src/webview_overlay_coords.cc`.
const double kOverlayStaleViewEpsilon = 1.5;

bool overlayViewSizeIsStale({
  required double flutterViewW,
  required double flutterViewH,
  required double flViewAllocW,
  required double flViewAllocH,
}) {
  if (flutterViewW <= 1.0 || flutterViewH <= 1.0) return false;
  if (flViewAllocW <= 0 || flViewAllocH <= 0) return false;
  return (flutterViewW - flViewAllocW).abs() > kOverlayStaleViewEpsilon ||
      (flutterViewH - flViewAllocH).abs() > kOverlayStaleViewEpsilon;
}

/// Native `webview_overlay_intersect_host` counterpart.
({int x, int y, int w, int h, bool nonEmpty}) overlayIntersectHost({
  required int x,
  required int y,
  required int w,
  required int h,
  required int hostW,
  required int hostH,
}) {
  final left = math.max(0, x);
  final top = math.max(0, y);
  final right = math.min(hostW, x + w);
  final bottom = math.min(hostH, y + h);
  final nonEmpty = right > left && bottom > top;
  return (
    x: nonEmpty ? left : 0,
    y: nonEmpty ? top : 0,
    w: nonEmpty ? right - left : 1,
    h: nonEmpty ? bottom - top : 1,
    nonEmpty: nonEmpty,
  );
}

/// Measures a placeholder [RenderBox] into FlView-local logical pixels.
///
/// Uses the transformed bounding box (handles [Transform] / scale ancestors)
/// divided by DPR, then intersects ancestor paint clips and the view.
Rect? measureFlViewLogicalRect({
  required RenderBox placeholder,
  required RenderView renderView,
}) {
  return measureFlViewVisibleRect(
    placeholder: placeholder,
    renderView: renderView,
  );
}

/// Visible FlView-local logical rect (transformed + clipped).
Rect? measureFlViewVisibleRect({
  required RenderBox placeholder,
  required RenderView renderView,
}) {
  if (!placeholder.hasSize || !placeholder.attached) return null;
  final dpr = renderView.flutterView.devicePixelRatio;
  if (dpr <= 0) return null;

  final transform = placeholder.getTransformTo(renderView);
  final physical = MatrixUtils.transformRect(
    transform,
    Offset.zero & placeholder.size,
  );
  var logical = Rect.fromLTRB(
    physical.left / dpr,
    physical.top / dpr,
    physical.right / dpr,
    physical.bottom / dpr,
  );

  RenderObject child = placeholder;
  RenderObject? node = placeholder.parent;
  while (node != null && node != renderView) {
    final paintClip = node.describeApproximatePaintClip(child);
    if (paintClip != null && node is RenderBox && node.hasSize) {
      final clipPhysical = MatrixUtils.transformRect(
        node.getTransformTo(renderView),
        paintClip,
      );
      final clipLogical = Rect.fromLTRB(
        clipPhysical.left / dpr,
        clipPhysical.top / dpr,
        clipPhysical.right / dpr,
        clipPhysical.bottom / dpr,
      );
      logical = logical.intersect(clipLogical);
      if (logical.isEmpty) return null;
    }
    child = node;
    node = node.parent;
  }

  final view = Offset.zero & renderView.size;
  logical = logical.intersect(view);
  if (logical.isEmpty || logical.width <= 0 || logical.height <= 0) {
    return null;
  }
  return logical;
}

/// Subtracts axis-aligned occluder strips; hides on messy overlap.
Rect applyOverlayOccluders(Rect slot, Iterable<Rect> occluders) {
  var result = slot;
  for (final occluder in occluders) {
    final hit = result.intersect(occluder);
    if (hit.isEmpty) continue;
    final slotArea = result.width * result.height;
    final hitArea = hit.width * hit.height;
    if (slotArea <= 0 || hitArea >= slotArea * 0.5) {
      return Rect.zero;
    }
    if (hit.height < result.height * 0.35 &&
        hit.width >= result.width * 0.8) {
      if ((hit.top - result.top).abs() < 1.0) {
        result = Rect.fromLTRB(
          result.left,
          hit.bottom,
          result.right,
          result.bottom,
        );
      } else if ((hit.bottom - result.bottom).abs() < 1.0) {
        result = Rect.fromLTRB(
          result.left,
          result.top,
          result.right,
          hit.top,
        );
      } else {
        return Rect.zero;
      }
    } else if (hit.width < result.width * 0.35 &&
        hit.height >= result.height * 0.8) {
      if ((hit.left - result.left).abs() < 1.0) {
        result = Rect.fromLTRB(
          hit.right,
          result.top,
          result.right,
          result.bottom,
        );
      } else if ((hit.right - result.right).abs() < 1.0) {
        result = Rect.fromLTRB(
          result.left,
          result.top,
          hit.left,
          result.bottom,
        );
      } else {
        return Rect.zero;
      }
    } else {
      return Rect.zero;
    }
    if (result.isEmpty || result.width < 80 || result.height < 80) {
      return Rect.zero;
    }
  }
  return result;
}

/// Resolves the [RenderView] for [context]'s Flutter view (multi-window safe).
RenderView? renderViewForContext(BuildContext context) {
  final flutterView = View.maybeOf(context);
  if (flutterView != null) {
    for (final renderView in RendererBinding.instance.renderViews) {
      if (renderView.flutterView.viewId == flutterView.viewId) {
        return renderView;
      }
    }
  }
  final views = RendererBinding.instance.renderViews;
  return views.isEmpty ? null : views.first;
}

/// Round rectangle edges then derive width/height (logical or physical).
Rect roundRectEdges(Rect rect) {
  final left = rect.left.roundToDouble();
  final top = rect.top.roundToDouble();
  final right = rect.right.roundToDouble();
  final bottom = rect.bottom.roundToDouble();
  return Rect.fromLTRB(
    left,
    top,
    right < left ? left : right,
    bottom < top ? top : bottom,
  );
}
