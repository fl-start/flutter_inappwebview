import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_keep_alive_pool.dart';
import 'webkitgtk_overlay_hooks.dart';
import 'webview_controller_webkitgtk.dart';

/// Toggle for verbose Linux WebKit z-order tracing in the terminal.
const bool kWebKitZOrderTrace = false;

void _wkz(int viewId, String msg) {
  if (!kWebKitZOrderTrace) return;
  debugPrint('🐧[WKZ v$viewId] $msg');
}

/// GtkOverlay-hosted WebKitGTK surface for Linux.
///
/// Z-order: native GTK overlay paints above Flutter; clip bounds via layout.
class WebKitGtkOverlayWidget extends StatefulWidget {
  final String? initialUrl;
  final String? initialHtml;
  final String? initialHtmlBaseUrl;
  final List<UserScript>? initialUserScripts;
  final Map<String, dynamic>? initialSettings;
  final InAppWebViewKeepAlive? keepAlive;
  final void Function(String url) onLoadStart;
  final void Function(String url) onLoadStop;
  final void Function(String url, int code, String message) onLoadError;
  final Future<int> Function(String url) shouldOverrideUrlLoading;
  final Future<dynamic> Function(String name, dynamic payload) onMessage;
  final void Function(WebViewControllerWebKitGTK controller) onWebViewCreated;
  final Map<String, String>? initialHeaders;

  const WebKitGtkOverlayWidget({
    super.key,
    this.initialUrl,
    this.initialHtml,
    this.initialHtmlBaseUrl,
    this.initialUserScripts,
    this.initialSettings,
    this.keepAlive,
    required this.onLoadStart,
    required this.onLoadStop,
    required this.onLoadError,
    required this.shouldOverrideUrlLoading,
    required this.onMessage,
    required this.onWebViewCreated,
    this.initialHeaders,
  });

  @override
  State<WebKitGtkOverlayWidget> createState() => _WebKitGtkOverlayWidgetState();
}

class _WebKitGtkOverlayWidgetState extends State<WebKitGtkOverlayWidget>
    with WidgetsBindingObserver {
  static int _nextViewId = 1;
  late final int _viewId;
  WebViewControllerWebKitGTK? _controller;
  bool _isInitialized = false;
  // Start hidden until first successful bounds sync; avoids a brief flash at
  // (0,0) or stale keep-alive geometry before the placeholder is measured.
  bool _nativeVisible = false;
  bool _loggedRoute = false;
  String? _loadError;
  final GlobalKey _placeholderKey = GlobalKey();
  VoidCallback? _overlayGeometryListener;
  VoidCallback? _modalScopeListener;
  VoidCallback? _forceSyncHandler;
  ({
    double x,
    double y,
    double width,
    double height,
    double viewWidth,
    double viewHeight,
    double dpr,
  })?
  _lastSentBounds;
  ({
    double x,
    double y,
    double width,
    double height,
    double viewWidth,
    double viewHeight,
    double dpr,
  })?
  _pendingBounds;
  bool _boundsSendScheduled = false;
  Size? _lastLayoutConstraints;
  Size? _pendingMeasurementSize;
  Offset? _pendingMeasurementOrigin;
  int _stableMeasurementFrames = 0;

  static bool _boundsNearlyEqual(
    ({
      double x,
      double y,
      double width,
      double height,
      double viewWidth,
      double viewHeight,
      double dpr,
    })
    a,
    ({
      double x,
      double y,
      double width,
      double height,
      double viewWidth,
      double viewHeight,
      double dpr,
    })
    b,
  ) {
    const epsilon = 0.5;
    return (a.x - b.x).abs() < epsilon &&
        (a.y - b.y).abs() < epsilon &&
        (a.width - b.width).abs() < epsilon &&
        (a.height - b.height).abs() < epsilon &&
        (a.viewWidth - b.viewWidth).abs() < epsilon &&
        (a.viewHeight - b.viewHeight).abs() < epsilon &&
        (a.dpr - b.dpr).abs() < 0.01;
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _viewId = WebKitGtkKeepAlivePool.viewIdFor(widget.keepAlive?.id) ??
        _nextViewId++;
    _wkz(_viewId, 'initState');
    WebKitGtkChannelDispatcher.registerView(_viewId, _handleMethodCall);
    _initializeWebView();
    _overlayGeometryListener = () {
      _lastSentBounds = null;
      _pendingMeasurementSize = null;
      _pendingMeasurementOrigin = null;
      _stableMeasurementFrames = 0;
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
      _scheduleNativeBoundsSync(frames: 5);
      unawaited(
        Future<void>.delayed(const Duration(milliseconds: 600), () {
          WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
        }),
      );
    };
    WebKitGtkOverlayHooks.layoutEpoch.addListener(_overlayGeometryListener!);
    _forceSyncHandler = () {
      if (!mounted) return;
      _lastSentBounds = null;
      WebKitGtkOverlayHooks.activeEmbeddedViewId = _viewId;
      _syncNativeWindowPosition(bypassDebounce: true);
    };
    WebKitGtkOverlayHooks.registerSyncHandler(_forceSyncHandler!);
    // Re-evaluate native visibility whenever a root-navigator dialog/popup
    // (e.g. the compose dialog) opens or closes. Needed because shell-nested
    // webviews (the email reader) do not rebuild when a root dialog is pushed.
    _modalScopeListener = _reevaluateVisibility;
    WebKitGtkOverlayHooks.rootPopupCount.addListener(_modalScopeListener!);
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (!_loggedRoute) {
      _loggedRoute = true;
      final route = ModalRoute.of(context);
      final rootNav = Navigator.maybeOf(context, rootNavigator: true);
      final onRoot = route?.navigator == rootNav;
      _wkz(
        _viewId,
        'route=${route?.runtimeType} isCurrent=${route?.isCurrent} '
        'onRootNavigator=$onRoot',
      );
    }
    // A modal opening/closing on the root navigator can change our occlusion
    // state without rebuilding us; re-check now that dependencies are settled.
    _reevaluateVisibility();
  }

  /// Whether the native surface should currently be on-screen.
  ///
  /// Hidden when: this state is gone, the subtree is offstage, our route is not
  /// current, OR a modal is stacked above the base shell route on the root
  /// navigator while THIS webview lives below it (shell-nested, e.g. the email
  /// reader). Webviews that live inside the topmost root route (the dialog
  /// itself, e.g. the composer editor) stay visible.
  bool _computeShouldBeVisible() {
    if (!mounted) return false;
    if (!TickerMode.valuesOf(context).enabled) return false;
    final route = ModalRoute.of(context);
    if (route != null && !route.isCurrent) return false;
    if (WebKitGtkOverlayHooks.isAnyRootPopupOpen) {
      final rootNav = Navigator.maybeOf(context, rootNavigator: true);
      final isOnRootNav = route?.navigator == rootNav;
      // Shell-nested webviews (email reader) hide under any root dialog/popup.
      // Webviews inside the dialog itself (composer editor) stay visible.
      if (!isOnRootNav) return false;
    }
    // Tiny / near-zero slots (prewarm opacity placeholders) must stay natively
    // hidden — Opacity does not hide GtkOverlay WebKit.
    final box = context.findRenderObject() as RenderBox?;
    if (box != null &&
        box.hasSize &&
        (box.size.width < 80 || box.size.height < 80)) {
      return false;
    }
    return true;
  }

  void _reevaluateVisibility() {
    if (!mounted) return;
    final shouldBeVisible = _computeShouldBeVisible();
    if (_nativeVisible != shouldBeVisible) {
      _wkz(
        _viewId,
        'reevaluate -> ${shouldBeVisible ? "show" : "hide"} '
        '(rootPopupOpen=${WebKitGtkOverlayHooks.isAnyRootPopupOpen} '
        'popups=${WebKitGtkOverlayHooks.rootPopupCount.value})',
      );
      _nativeVisible = shouldBeVisible;
      _setNativeVisibility(shouldBeVisible);
    }
    if (shouldBeVisible) {
      _scheduleNativeBoundsSync(frames: 3);
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    WebKitGtkChannelDispatcher.unregisterView(_viewId);
    if (_forceSyncHandler != null) {
      WebKitGtkOverlayHooks.unregisterSyncHandler(_forceSyncHandler!);
      _forceSyncHandler = null;
    }
    if (WebKitGtkOverlayHooks.activeEmbeddedViewId == _viewId) {
      WebKitGtkOverlayHooks.activeEmbeddedViewId = null;
    }
    if (_overlayGeometryListener != null) {
      WebKitGtkOverlayHooks.layoutEpoch.removeListener(
        _overlayGeometryListener!,
      );
    }
    if (_modalScopeListener != null) {
      WebKitGtkOverlayHooks.rootPopupCount.removeListener(_modalScopeListener!);
      _modalScopeListener = null;
    }
    _pendingBounds = null;
    _nativeVisible = false;
    _setNativeVisibility(false);
    _disposeWebView();
    super.dispose();
  }

  @override
  void deactivate() {
    // When this subtree is moved offstage or replaced by a different flow
    // (e.g. settings), ensure native overlay is hidden immediately.
    _nativeVisible = false;
    _setNativeVisibility(false);
    super.deactivate();
  }

  @override
  void activate() {
    super.activate();
    // Re-evaluate rather than force-show: a modal may still cover us.
    _reevaluateVisibility();
  }

  @override
  void didChangeMetrics() {
    _lastSentBounds = null;
    _pendingMeasurementSize = null;
    _pendingMeasurementOrigin = null;
    _stableMeasurementFrames = 0;
    WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
    _reevaluateVisibility();
    _scheduleNativeBoundsSync(frames: 8);
    // Compositor/GTK allocation often lags Flutter metrics on maximize.
    for (final delay in const [
      Duration(milliseconds: 16),
      Duration(milliseconds: 48),
      Duration(milliseconds: 96),
      Duration(milliseconds: 200),
      Duration(milliseconds: 400),
      Duration(milliseconds: 700),
    ]) {
      unawaited(
        Future<void>.delayed(delay, () {
          if (!mounted) return;
          _lastSentBounds = null;
          _reevaluateVisibility();
          _syncNativeWindowPosition(bypassDebounce: true);
        }),
      );
    }
    unawaited(
      Future<void>.delayed(const Duration(milliseconds: 800), () {
        WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
      }),
    );
  }

  RenderView? _renderViewFor(BuildContext context) {
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

  void _scheduleNativeBoundsSync({int frames = 1}) {
    void run(int remaining) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) return;
        _syncNativeWindowPosition(bypassDebounce: true);
        if (remaining > 1) {
          run(remaining - 1);
        }
      });
    }

    run(frames);
  }

  Future<dynamic> _handleMethodCall(MethodCall call) async {
    switch (call.method) {
      case 'onLoadStart':
        final url = call.arguments['url'] as String;
        widget.onLoadStart(url);
        break;
      case 'onLoadStop':
        final url = call.arguments['url'] as String;
        widget.onLoadStop(url);
        break;
      case 'onLoadError':
        final url = call.arguments['url'] as String;
        final code = call.arguments['code'] as int;
        final message = call.arguments['message'] as String;
        setState(() {
          _loadError = 'Error loading $url (Code: $code): $message';
        });
        widget.onLoadError(url, code, message);
        break;
      case 'shouldOverrideUrlLoading':
        final url = call.arguments['url'] as String;
        return widget.shouldOverrideUrlLoading(url);
      case 'onMessage':
        final name = call.arguments['name'] as String;
        final payload = call.arguments['payload'];
        return widget.onMessage(name, payload);
      case 'onHostLayoutChanged':
        _lastSentBounds = null;
        WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
        _reevaluateVisibility();
        _syncNativeWindowPosition(bypassDebounce: true);
        _scheduleNativeBoundsSync(frames: 4);
        unawaited(
          Future<void>.delayed(const Duration(milliseconds: 500), () {
            WebKitGtkOverlayHooks.forceImmediateBoundsSync = false;
          }),
        );
        break;
      default:
        throw MissingPluginException();
    }
  }

  Future<void> _initializeWebView() async {
    try {
      final keepAliveId = widget.keepAlive?.id;
      final reused = WebKitGtkKeepAlivePool.controllerFor(keepAliveId);
      if (reused != null) {
        _controller = reused;
        widget.onWebViewCreated(_controller!);
        await WebKitGtkChannelDispatcher.channel.invokeMethod('show', {'viewId': _viewId});
        setState(() => _isInitialized = true);
        _scheduleNativeBoundsSync(frames: 2);
        return;
      }

      await WebKitGtkChannelDispatcher.channel.invokeMethod('create', {
        'viewId': _viewId,
        if (widget.initialSettings != null) 'settings': widget.initialSettings,
        if (widget.initialUserScripts != null)
          'userScripts': widget.initialUserScripts!
              .map((script) => script.toMap())
              .toList(),
      });

      _controller = WebViewControllerWebKitGTK(_viewId);
      widget.onWebViewCreated(_controller!);

      await _controller!.addJavaScriptChannel('emailComposer');
      await _controller!.addJavaScriptChannel('openExternalUrl');

      if (widget.initialUrl != null) {
        await _controller!.loadUrl(
          widget.initialUrl!,
          headers: widget.initialHeaders,
        );
      } else if (widget.initialHtml != null) {
        await _controller!.loadHtml(
          widget.initialHtml!,
          baseUrl: widget.initialHtmlBaseUrl,
        );
      }

      setState(() => _isInitialized = true);
      _scheduleNativeBoundsSync(frames: 2);
    } catch (e) {
      setState(() => _loadError = e.toString());
      widget.onLoadError('', -1, 'Failed to initialize WebView: $e');
    }
  }

  Future<void> _disposeWebView() async {
    if (_controller == null) return;
    final keepAliveId = widget.keepAlive?.id;
    try {
      await _controller!.dispose(keepAlive: keepAliveId != null);
      if (keepAliveId != null) {
        WebKitGtkKeepAlivePool.store(
          keepAliveId: keepAliveId,
          viewId: _viewId,
          controller: _controller!,
        );
      }
    } catch (_) {}
  }

  Future<void> _setNativeVisibility(bool visible) async {
    if (!_isInitialized) return;
    if (_controller == null) return;
    if (!visible) {
      // Drop any queued bounds so the debounced send can't re-show us.
      _pendingBounds = null;
    }
    _wkz(_viewId, 'native ${visible ? "SHOW" : "HIDE"}');
    try {
      await WebKitGtkChannelDispatcher.channel.invokeMethod(visible ? 'show' : 'hide', {
        'viewId': _viewId,
      });
    } catch (_) {
      // Best effort visibility sync.
    }
  }

  @override
  Widget build(BuildContext context) {
    // Keep native WebKit visible only while this route is the top-most route.
    // Flutter dialogs/popups are rendered above the route in Flutter's overlay,
    // but native GTK surfaces always paint above Flutter. Hiding native WebKit
    // during popup routes prevents the WebKit surface from covering dialogs.
    final shouldBeVisible = _computeShouldBeVisible();
    if (_nativeVisible != shouldBeVisible) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) return;
        final next = _computeShouldBeVisible();
        if (_nativeVisible == next) return;
        _wkz(_viewId, 'build -> ${next ? "show" : "hide"}');
        _nativeVisible = next;
        _setNativeVisibility(_nativeVisible);
      });
    }

    if (!_isInitialized) {
      return Container(
        color: Colors.grey.shade200,
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const CircularProgressIndicator(),
              const SizedBox(height: 16),
              Text(
                'Initializing WebView...',
                style: Theme.of(context).textTheme.bodyMedium,
              ),
            ],
          ),
        ),
      );
    }

    if (_loadError != null) {
      return Container(
        color: Colors.grey.shade200,
        child: Center(
          child: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const Icon(Icons.error_outline, size: 48, color: Colors.red),
                const SizedBox(height: 16),
                Text(
                  'WebView Error',
                  style: Theme.of(context).textTheme.titleLarge,
                ),
                const SizedBox(height: 8),
                Text(
                  _loadError!,
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
              ],
            ),
          ),
        ),
      );
    }

    // Placeholder reserves layout space; native WebKit is embedded via setBounds.
    // Do not add a Flutter Overlay on top — it intercepts mouse-wheel input.
    final placeholderColor = Theme.of(context).colorScheme.surface;

    return LayoutBuilder(
      builder: (context, constraints) {
        final nextConstraints = Size(
          constraints.maxWidth,
          constraints.maxHeight,
        );
        if (_lastLayoutConstraints == null ||
            _lastLayoutConstraints != nextConstraints) {
          _lastLayoutConstraints = nextConstraints;
          _lastSentBounds = null;
          _scheduleNativeBoundsSync(frames: 3);
        }

        return MouseRegion(
          onEnter: (_) => _syncNativeWindowPosition(bypassDebounce: true),
          child: Listener(
            behavior: HitTestBehavior.translucent,
            onPointerDown: (_) => unawaited(_grabNativeFocus()),
            child: Container(
              key: _placeholderKey,
              color: placeholderColor,
              width: constraints.maxWidth > 0
                  ? constraints.maxWidth
                  : double.infinity,
              height: constraints.maxHeight > 0
                  ? constraints.maxHeight
                  : double.infinity,
            ),
          ),
        );
      },
    );
  }

  Future<void> _grabNativeFocus() async {
    if (!_isInitialized) return;
    try {
      await _controller?.grabFocus();
    } catch (_) {}
  }

  void _syncNativeWindowPosition({bool bypassDebounce = false}) {
    if (!_isInitialized) return;

    final shouldBeVisible = _computeShouldBeVisible();
    if (_nativeVisible != shouldBeVisible) {
      _nativeVisible = shouldBeVisible;
      _setNativeVisibility(shouldBeVisible);
    }

    final Rect? hostBounds = WebKitGtkOverlayHooks.boundsProvider?.call();
    final force = bypassDebounce ||
        WebKitGtkOverlayHooks.forceImmediateBoundsSync ||
        hostBounds != null;

    // Still push geometry while hidden during maximize: a stale !_nativeVisible
    // gate previously skipped the only setBounds that would relocate the surface,
    // leaving WebKit stuck at pre-maximize coords while Flutter had already grown.
    if (!_nativeVisible && !force) return;

    final placeholderContext = _placeholderKey.currentContext;
    RenderBox? placeholderBox;
    RenderView? renderView;
    if (placeholderContext != null) {
      placeholderBox = placeholderContext.findRenderObject() as RenderBox?;
      renderView = _renderViewFor(placeholderContext);
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
        // localToGlobal(ancestor: renderView) lands in RenderView's own
        // coordinate space, which is physical/device pixels (RenderView hands
        // physical-pixel layers to the engine) — not Flutter's usual logical
        // pixels. Divide by dpr to get back to the logical/GTK-allocation units
        // the native side (and host_w/host_h clamping) expects. At dpr=1.0 this
        // is a no-op, which is why the bug was invisible except on non-1.0
        // (including non-integer, e.g. KDE 125%) display scaling.
        final localDpr = renderView.flutterView.devicePixelRatio;
        final Offset rawOffset = placeholderBox.localToGlobal(
          Offset.zero,
          ancestor: renderView,
        );
        overlayOffset = localDpr > 0
            ? Offset(rawOffset.dx / localDpr, rawOffset.dy / localDpr)
            : rawOffset;
        size = placeholderBox.size;
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
        final sizeStable = _pendingMeasurementSize != null &&
            (_pendingMeasurementSize!.width - measurement.width).abs() < 0.5 &&
            (_pendingMeasurementSize!.height - measurement.height).abs() < 0.5;
        final originStable = _pendingMeasurementOrigin != null &&
            (_pendingMeasurementOrigin!.dx - origin.dx).abs() < 0.5 &&
            (_pendingMeasurementOrigin!.dy - origin.dy).abs() < 0.5;
        if (sizeStable && originStable) {
          _stableMeasurementFrames++;
        } else {
          _pendingMeasurementSize = measurement;
          _pendingMeasurementOrigin = origin;
          _stableMeasurementFrames = 0;
        }
        if (_stableMeasurementFrames < 1) {
          _scheduleNativeBoundsSync(frames: 1);
          return;
        }
      }

      final bounds = (
        x: overlayOffset.dx,
        y: overlayOffset.dy,
        width: effectiveWidth,
        height: effectiveHeight,
        viewWidth: viewWidth,
        viewHeight: viewHeight,
        dpr: dpr,
      );
      if (!force &&
          _lastSentBounds != null &&
          _boundsNearlyEqual(_lastSentBounds!, bounds)) {
        return;
      }

      void sendBounds(
        ({
          double x,
          double y,
          double width,
          double height,
          double viewWidth,
          double viewHeight,
          double dpr,
        })
        target,
      ) {
        if (!mounted) return;
        // Do not re-check !_nativeVisible here — maximize repair must land even
        // when visibility flipped mid-frame; show/hide is handled separately.
        _pendingBounds = null;
        _lastSentBounds = target;
        WebKitGtkOverlayHooks.activeEmbeddedViewId = _viewId;
        WebKitGtkOverlayHooks.onNativeBoundsSent?.call(
          x: target.x,
          y: target.y,
          width: target.width,
          height: target.height,
        );
        _wkz(
          _viewId,
          'setBounds x=${target.x.round()},y=${target.y.round()} '
          'size=${target.width.round()}x${target.height.round()} '
          'view=${target.viewWidth.round()}x${target.viewHeight.round()} '
          'dpr=${target.dpr.toStringAsFixed(2)} '
          'visible=$_nativeVisible',
        );
        WebKitGtkChannelDispatcher.channel.invokeMethod('setBounds', {
          'viewId': _viewId,
          'x': target.x,
          'y': target.y,
          'width': target.width,
          'height': target.height,
          'viewWidth': target.viewWidth,
          'viewHeight': target.viewHeight,
          'devicePixelRatio': target.dpr,
        });
        if (_nativeVisible) {
          // Ensure surface is shown after bounds land (hide may have raced).
          unawaited(_setNativeVisibility(true));
        }
      }

      if (force) {
        _pendingBounds = null;
        sendBounds(bounds);
      } else {
        _pendingBounds = bounds;
        if (_boundsSendScheduled) return;
        _boundsSendScheduled = true;
        WidgetsBinding.instance.addPostFrameCallback((_) {
          _boundsSendScheduled = false;
          final target = _pendingBounds;
          if (!mounted || target == null) return;
          sendBounds(target);
        });
      }
    } catch (e, st) {
      if (kWebKitZOrderTrace) {
        debugPrint('🐧[WKZ v$_viewId] COORD sync failed: $e\n$st');
      }
    }
  }
}
