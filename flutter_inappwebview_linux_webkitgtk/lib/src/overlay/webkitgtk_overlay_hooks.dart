import 'package:flutter/foundation.dart';

/// Optional host-app hooks for GtkOverlay geometry and modal occlusion.
///
/// secMail wires [rightInset], [layoutEpoch], and [rootPopupCount] at startup.
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
    layoutEpoch.value++;
  }
}
