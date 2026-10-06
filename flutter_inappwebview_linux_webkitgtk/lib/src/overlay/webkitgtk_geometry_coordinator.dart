import 'package:flutter/foundation.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/widgets.dart';

import 'webkitgtk_geometry.dart';
import 'webkitgtk_overlay_hooks.dart';

/// Host-published Flutter overlays (toasts) that paint above the reader slot
/// but under native WebKit. Rects are FlView-local logical pixels.
abstract final class WebKitGtkOverlayOccluders {
  static final Map<Object, Rect> _rects = <Object, Rect>{};

  static List<Rect> get rects => _rects.values.toList(growable: false);

  static void publish(Object key, Rect rect) {
    final previous = _rects[key];
    if (previous != null &&
        (previous.left - rect.left).abs() < 0.5 &&
        (previous.top - rect.top).abs() < 0.5 &&
        (previous.width - rect.width).abs() < 0.5 &&
        (previous.height - rect.height).abs() < 0.5) {
      return;
    }
    _rects[key] = rect;
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  static void remove(Object key) {
    if (_rects.remove(key) == null) return;
    WebKitGtkGeometryCoordinator.instance.schedule();
  }

  @visibleForTesting
  static void clearForTest() {
    _rects.clear();
  }
}

/// One post-frame geometry pass per FlutterView.
///
/// Overlay widgets register a [VoidCallback] that measures their placeholder
/// and sends at most one `setBounds`. App delay tables must not exist; call
/// [schedule] or [WebKitGtkOverlayHooks.forceSyncAll] instead.
class WebKitGtkGeometryCoordinator {
  WebKitGtkGeometryCoordinator._();

  static final WebKitGtkGeometryCoordinator instance =
      WebKitGtkGeometryCoordinator._();

  final List<VoidCallback> _clients = <VoidCallback>[];
  bool _scheduled = false;

  void register(VoidCallback client) {
    if (_clients.contains(client)) return;
    _clients.add(client);
    schedule();
  }

  void unregister(VoidCallback client) {
    _clients.remove(client);
  }

  /// Coalesce to the next frame. Safe to call from layout / metrics / native
  /// `onHostLayoutChanged`.
  void schedule() {
    if (_scheduled) return;
    _scheduled = true;
    final binding = SchedulerBinding.instance;
    binding.scheduleFrame();
    binding.addPostFrameCallback((_) {
      _scheduled = false;
      flush();
    });
  }

  void flush() {
    for (final client in _clients.toList(growable: false)) {
      client();
    }
    // One-shot: callers set forceImmediateBoundsSync to bypass the stability
    // gate for this pass only. Leaving it set disabled the gate and the
    // unchanged-geometry dedupe permanently (a setBounds per metrics tick).
    WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
  }

  @visibleForTesting
  int get clientCount => _clients.length;
}

/// Wraps [child] and publishes its FlView-local rect as an occluder.
class WebKitGtkOverlayOccluder extends StatefulWidget {
  const WebKitGtkOverlayOccluder({
    super.key,
    required this.child,
    Object? id,
  }) : id = id;

  final Widget child;
  final Object? id;

  @override
  State<WebKitGtkOverlayOccluder> createState() =>
      _WebKitGtkOverlayOccluderState();
}

class _WebKitGtkOverlayOccluderState extends State<WebKitGtkOverlayOccluder> {
  late final Object _id = widget.id ?? this;

  @override
  void dispose() {
    WebKitGtkOverlayOccluders.remove(_id);
    super.dispose();
  }

  void _publish() {
    final box = context.findRenderObject();
    if (box is! RenderBox || !box.hasSize) return;
    final renderView = renderViewForContext(context);
    if (renderView == null) return;
    final rect = measureFlViewVisibleRect(
      placeholder: box,
      renderView: renderView,
    );
    if (rect == null) {
      WebKitGtkOverlayOccluders.remove(_id);
      return;
    }
    WebKitGtkOverlayOccluders.publish(_id, rect);
  }

  @override
  Widget build(BuildContext context) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) _publish();
    });
    return widget.child;
  }
}
