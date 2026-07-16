import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'webkitgtk_channel_dispatcher.dart';

/// Optional host-app hooks for GtkOverlay geometry and modal occlusion.
///
/// secMail wires [rightInset], [layoutEpoch], [rootPopupCount], and
/// [boundsProvider] at startup.
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

  /// Optional host-provided reader bounds in logical Flutter-view coordinates.
  ///
  /// When set, the Linux overlay can use these bounds during resize/maximize
  /// instead of relying only on the internal placeholder, which may lag a frame.
  static Rect? Function()? boundsProvider;

  /// When true, overlay widgets skip the "wait for stable placeholder" gate and
  /// push [setBounds] immediately (used during maximize/restore).
  static bool forceImmediateBoundsSync = false;

  /// Currently visible embedded reader view id (for host-driven setBounds repair).
  static int? activeEmbeddedViewId;

  static final List<VoidCallback> _syncHandlers = <VoidCallback>[];

  static void registerSyncHandler(VoidCallback handler) {
    _syncHandlers.add(handler);
  }

  static void unregisterSyncHandler(VoidCallback handler) {
    _syncHandlers.remove(handler);
  }

  /// Ask every mounted overlay to push setBounds now (maximize / pane resize).
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
  static Future<void> pushSetBounds({
    required double x,
    required double y,
    required double width,
    required double height,
    required double viewWidth,
    required double viewHeight,
    required double devicePixelRatio,
    int? viewId,
  }) async {
    final id = viewId ?? activeEmbeddedViewId;
    if (id == null) return;
    if (width <= 0 || height <= 0) return;

    onNativeBoundsSent?.call(x: x, y: y, width: width, height: height);
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;

    await WebKitGtkChannelDispatcher.channel.invokeMethod('setBounds', {
      'viewId': id,
      'x': x,
      'y': y,
      'width': width,
      'height': height,
      'viewWidth': viewWidth,
      'viewHeight': viewHeight,
      'devicePixelRatio': devicePixelRatio,
    });
  }
}
