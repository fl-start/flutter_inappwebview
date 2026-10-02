import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_geometry.dart';
import 'webkitgtk_geometry_coordinator.dart';
import 'webkitgtk_native_health.dart';

/// Optional host-app hooks for GtkOverlay geometry and modal occlusion.
///
/// secMail wires [rightInset], [layoutEpoch], [rootPopupCount],
/// [exclusiveShellViewId], and [boundsProvider] at startup.
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

  /// When non-null, only this shell-nested overlay [viewId] may be natively
  /// visible. Other shell-nested overlays are forced hidden.
  ///
  /// Used for in-shell workspace tabs (compose vs reader) where there is no
  /// root PopupRoute to drive [rootPopupCount]. Overlays hosted on a covering
  /// root route are unaffected. `null` disables exclusive mode.
  static final ValueNotifier<int?> exclusiveShellViewId =
      ValueNotifier<int?>(null);

  static void setExclusiveShellViewId(int? viewId) {
    if (exclusiveShellViewId.value == viewId) return;
    exclusiveShellViewId.value = viewId;
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  static void setRightInset(double value) {
    final normalized = value < 0 ? 0.0 : value;
    if ((rightInset.value - normalized).abs() < 0.5) return;
    rightInset.value = normalized;
  }

  static void notifyLayoutChanged() {
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  /// Optional host callback when native [setBounds] is invoked (debug alignment).
  static void Function({
    required double x,
    required double y,
    required double width,
    required double height,
  })?
  onNativeBoundsSent;

  /// Optional host callback when native acknowledges a `setBounds`.
  static void Function({
    required bool applied,
    required int sequence,
    required double x,
    required double y,
    required double width,
    required double height,
  })?
  onNativeBoundsAck;

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
    WebKitGtkGeometryCoordinator.instance.register(handler);
  }

  static void unregisterSyncHandler(VoidCallback handler) {
    _syncHandlers.remove(handler);
    WebKitGtkGeometryCoordinator.instance.unregister(handler);
  }

  /// Ask every mounted overlay to push setBounds on the next frame.
  ///
  /// Each overlay measures **its own** placeholder (or host provider for its
  /// viewId). Handlers must not clobber [activeEmbeddedViewId].
  static void forceSyncAll() {
    forceImmediateBoundsSync = true;
    layoutEpoch.value++;
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  /// Last-resort host push when Flutter slot and native bounds diverge.
  ///
  /// Uses the shared monotonic sequence — never a wall-clock timestamp.
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
    int? generation,
    bool visible = true,
  }) async {
    final id = viewId ?? activeEmbeddedViewId;
    if (id == null) return;
    if (width <= 0 || height <= 0) return;

    final geometry = WebKitGtkOverlayGeometry(
      viewId: id,
      sequence: sequence ?? WebKitGtkGeometrySequence.next(),
      generation: generation ?? WebKitGtkGeometrySequence.generation,
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

    final raw = await WebKitGtkChannelDispatcher.channel.invokeMethod(
      'setBounds',
      geometry.toMethodChannelArgs(),
    );
    _reportAck(raw, fallback: geometry);
  }

  static void _reportAck(Object? raw, {required WebKitGtkOverlayGeometry fallback}) {
    var applied = true;
    var seq = fallback.sequence;
    var x = fallback.left;
    var y = fallback.top;
    var w = fallback.width;
    var h = fallback.height;
    if (raw is Map) {
      applied = raw['applied'] != false;
      final rawSeq = raw['seq'];
      if (rawSeq is num) seq = rawSeq.toInt();
      final rawX = raw['x'];
      final rawY = raw['y'];
      final rawW = raw['width'];
      final rawH = raw['height'];
      if (rawX is num) x = rawX.toDouble();
      if (rawY is num) y = rawY.toDouble();
      if (rawW is num) w = rawW.toDouble();
      if (rawH is num) h = rawH.toDouble();
    }
    if (applied) {
      onNativeBoundsSent?.call(x: x, y: y, width: w, height: h);
    }
    onNativeBoundsAck?.call(
      applied: applied,
      sequence: seq,
      x: x,
      y: y,
      width: w,
      height: h,
    );
  }

  /// Latest native create/load snapshot (Linux WebKitGTK).
  static WebKitGtkNativeHealth? lastNativeHealth;

  /// Called whenever native create/getNativeHealth reports a snapshot.
  static void Function(WebKitGtkNativeHealth health)? onNativeHealth;

  static void reportNativeHealth(WebKitGtkNativeHealth health) {
    lastNativeHealth = health;
    onNativeHealth?.call(health);
  }

  static Future<WebKitGtkNativeHealth?> queryNativeHealth({int? viewId}) async {
    final raw = await WebKitGtkChannelDispatcher.channel.invokeMethod(
      'getNativeHealth',
      {if (viewId != null) 'viewId': viewId},
    );
    final health = WebKitGtkNativeHealth.fromChannel(raw);
    reportNativeHealth(health);
    return health;
  }
}
