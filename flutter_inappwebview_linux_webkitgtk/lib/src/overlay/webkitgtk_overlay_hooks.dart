import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_geometry.dart';

/// Optional host-app hooks for GtkOverlay geometry and modal occlusion.
///
/// secMail wires [rightInset], [layoutEpoch], [rootPopupCount], and
/// [boundsProvider] at startup.
///
/// ## Coordinate contract
///
/// All geometry exchanged with native `setBounds` is **FlView-local logical
/// pixels** ([WebKitGtkOverlayGeometry.coordinateSpace]). Host
/// [boundsProvider] callbacks must return that same space — never raw
/// `localToGlobal(ancestor: renderView)` physical pixels without dividing by
/// DPR.
class WebKitGtkOverlayHooks {
  WebKitGtkOverlayHooks._();

  /// Legacy right inset; prefer clipping via Flutter layout (keep at 0).
  static final ValueNotifier<double> rightInset = ValueNotifier<double>(0);

  /// Increment to force native bounds re-sync after pane/window layout changes.
  static final ValueNotifier<int> layoutEpoch = ValueNotifier<int>(0);

  /// Root-navigator popup count (dialogs covering shell content).
  static final ValueNotifier<int> rootPopupCount = ValueNotifier<int>(0);

  static bool get isAnyRootPopupOpen => rootPopupCount.value > 0;

  static void setRightInset(double value) {
    final normalized = value < 0 ? 0.0 : value;
    if ((rightInset.value - normalized).abs() < 0.5) return;
    rightInset.value = normalized;
  }

  static void notifyLayoutChanged() {
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;
  }

  /// Optional host callback when native [setBounds] is invoked (debug alignment).
  static void Function({
    required double x,
    required double y,
    required double width,
    required double height,
  })?
  onNativeBoundsSent;

  /// Optional per-view host bounds in **FlView-local logical** pixels.
  ///
  /// Return `null` for views that should measure their own placeholder
  /// (composer, inactive keep-alives). Only accelerate the mailbox reader
  /// when its [viewId] matches — never share one rect across all overlays.
  static Rect? Function(int viewId)? boundsProvider;

  /// When true, overlay widgets skip the "wait for stable placeholder" gate and
  /// push [setBounds] immediately (used during maximize/restore).
  static bool forceImmediateBoundsSync = false;

  /// Currently visible embedded **reader** view id (host-driven setBounds repair).
  ///
  /// Only the mailbox reader should update this. Composer must not overwrite
  /// it, or maximize repair would push reader-slot geometry onto the composer.
  static int? activeEmbeddedViewId;

  static final List<VoidCallback> _syncHandlers = <VoidCallback>[];

  static void registerSyncHandler(VoidCallback handler) {
    _syncHandlers.add(handler);
  }

  static void unregisterSyncHandler(VoidCallback handler) {
    _syncHandlers.remove(handler);
  }

  /// Ask every mounted overlay to push setBounds now (maximize / pane resize).
  ///
  /// Each overlay measures **its own** placeholder (or host provider for its
  /// viewId). Handlers must not clobber [activeEmbeddedViewId].
  static void forceSyncAll() {
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;
    for (final handler in _syncHandlers.toList(growable: false)) {
      handler();
    }
  }

  /// Last-resort host push when Flutter slot and native bounds diverge.
  ///
  /// Bypasses overlay early-returns that previously left maximize stuck.
  /// [x]/[y]/[width]/[height] must be FlView-local logical pixels.
  static Future<void> pushSetBounds({
    required double x,
    required double y,
    required double width,
    required double height,
    required double viewWidth,
    required double viewHeight,
    required double devicePixelRatio,
    int? viewId,
    int? sequence,
    bool visible = true,
  }) async {
    final id = viewId ?? activeEmbeddedViewId;
    if (id == null) return;
    if (width <= 0 || height <= 0) return;

    final geometry = WebKitGtkOverlayGeometry(
      viewId: id,
      sequence: sequence ?? DateTime.now().microsecondsSinceEpoch,
      visible: visible,
      left: x,
      top: y,
      right: x + width,
      bottom: y + height,
      viewWidth: viewWidth,
      viewHeight: viewHeight,
      devicePixelRatio: devicePixelRatio,
    ).roundEdges();

    onNativeBoundsSent?.call(
      x: geometry.left,
      y: geometry.top,
      width: geometry.width,
      height: geometry.height,
    );
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;

    await WebKitGtkChannelDispatcher.channel.invokeMethod(
      'setBounds',
      geometry.toMethodChannelArgs(),
    );
  }
}
