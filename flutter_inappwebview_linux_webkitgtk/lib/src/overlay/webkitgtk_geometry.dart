import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

/// FlView-local logical geometry for GtkOverlay [setBounds].
///
/// Units: Flutter logical pixels relative to the [RenderView] / FlView origin.
/// Do **not** multiply by [devicePixelRatio] before sending — native maps
/// logical → GtkOverlay allocation via `flView_allocated / viewLogical`.
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
  });

  final int viewId;
  final int sequence;
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
      'WebKitGtkOverlayGeometry(v$viewId seq=$sequence '
      'visible=$visible '
      'LTRB=${left.toStringAsFixed(1)},${top.toStringAsFixed(1)},'
      '${right.toStringAsFixed(1)},${bottom.toStringAsFixed(1)} '
      'view=${viewWidth.toStringAsFixed(0)}x${viewHeight.toStringAsFixed(0)} '
      'dpr=${devicePixelRatio.toStringAsFixed(2)})';
}

/// Measures a placeholder [RenderBox] into FlView-local logical pixels.
///
/// [RenderBox.localToGlobal] with [ancestor] = [RenderView] returns coordinates
/// in the RenderView's **physical** layer space. Divide by DPR to recover the
/// logical units GTK allocation / FlView logical size use.
Rect? measureFlViewLogicalRect({
  required RenderBox placeholder,
  required RenderView renderView,
}) {
  if (!placeholder.hasSize || !placeholder.attached) return null;
  final dpr = renderView.flutterView.devicePixelRatio;
  final raw = placeholder.localToGlobal(Offset.zero, ancestor: renderView);
  final origin = dpr > 0 ? Offset(raw.dx / dpr, raw.dy / dpr) : raw;
  final size = placeholder.size;
  if (size.width <= 0 || size.height <= 0) return null;
  return origin & size;
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
