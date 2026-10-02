import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import 'webkitgtk_channel_dispatcher.dart';
import 'webkitgtk_custom_scheme.dart';
import 'webkitgtk_keep_alive_pool.dart';
import 'webkitgtk_native_health.dart';
import 'webkitgtk_overlay_bounds_sync.dart';
import 'webkitgtk_overlay_hooks.dart';
import 'webview_controller_webkitgtk.dart';

export 'webkitgtk_overlay_bounds_sync.dart' show kWebKitZOrderTrace;

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
  final Future<Map<String, dynamic>?> Function(WebResourceRequest request)?
      onLoadResourceWithCustomScheme;
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
    this.onLoadResourceWithCustomScheme,
    required this.onMessage,
    required this.onWebViewCreated,
    this.initialHeaders,
  });

  @override
  State<WebKitGtkOverlayWidget> createState() => _WebKitGtkOverlayWidgetState();
}

class _WebKitGtkOverlayWidgetState extends State<WebKitGtkOverlayWidget>
    with WidgetsBindingObserver, WebKitGtkOverlayBoundsSync {
  static int _nextViewId = 1;
  late final int _viewId;
  WebViewControllerWebKitGTK? _controller;
  bool _isInitialized = false;
  bool _loggedRoute = false;
  String? _loadError;
  final GlobalKey _placeholderKey = GlobalKey();
  VoidCallback? _overlayGeometryListener;
  VoidCallback? _modalScopeListener;
  VoidCallback? _forceSyncHandler;

  @override
  int get overlayViewId => _viewId;

  @override
  WebViewControllerWebKitGTK? get overlayController => _controller;

  @override
  bool get overlayInitialized => _isInitialized;

  @override
  GlobalKey get overlayPlaceholderKey => _placeholderKey;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _viewId = WebKitGtkKeepAlivePool.viewIdFor(widget.keepAlive?.id) ??
        _nextViewId++;
    claimOverlayGeneration();
    wkzTrace(_viewId, 'initState');
    WebKitGtkChannelDispatcher.registerView(_viewId, _handleMethodCall);
    _initializeWebView();
    _overlayGeometryListener = () {
      lastSentGeometry = null;
      lastAckedGeometry = null;
      pendingMeasurementSize = null;
      pendingMeasurementOrigin = null;
      stableMeasurementFrames = 0;
      WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
      scheduleNativeBoundsSync();
    };
    WebKitGtkOverlayHooks.layoutEpoch.addListener(_overlayGeometryListener!);
    _forceSyncHandler = () {
      if (!mounted) return;
      // Do not claim activeEmbeddedViewId here — forceSyncAll runs for every
      // mounted overlay (reader + composer). Each view measures itself.
      lastSentGeometry = null;
      syncNativeWindowPosition(bypassDebounce: true);
    };
    WebKitGtkOverlayHooks.registerSyncHandler(_forceSyncHandler!);
    // Re-evaluate native visibility whenever a root-navigator dialog/popup
    // (e.g. the compose dialog) opens or closes. Needed because shell-nested
    // webviews (the email reader) do not rebuild when a root dialog is pushed.
    _modalScopeListener = _reevaluateVisibility;
    WebKitGtkOverlayHooks.rootPopupCount.addListener(_modalScopeListener!);
    WebKitGtkOverlayHooks.exclusiveShellViewId.addListener(
      _modalScopeListener!,
    );
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (!_loggedRoute) {
      _loggedRoute = true;
      final route = ModalRoute.of(context);
      final rootNav = Navigator.maybeOf(context, rootNavigator: true);
      final onRoot = route?.navigator == rootNav;
      wkzTrace(
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
  /// current, OR a modal/fullscreen cover is stacked above the shell while THIS
  /// webview lives below it (shell-nested, e.g. the email reader). Webviews that
  /// live inside the covering route (composer / page-builder) stay visible.
  @override
  bool computeShouldBeVisible() {
    if (!mounted) return false;
    if (!TickerMode.valuesOf(context).enabled) return false;
    final route = ModalRoute.of(context);
    if (route != null && !route.isCurrent) return false;
    if (route != null && !route.isActive) return false;
    if (WebKitGtkOverlayHooks.isAnyRootPopupOpen) {
      final rootNav = Navigator.maybeOf(context, rootNavigator: true);
      final isOnRootNav = route?.navigator == rootNav;
      // Shell-nested webviews (email reader) hide under any root dialog/popup
      // or fullscreen shell-cover (compose MaterialPageRoute).
      // Webviews inside the cover itself (composer editor) stay visible.
      if (!isOnRootNav) return false;
    }
    final exclusiveId = WebKitGtkOverlayHooks.exclusiveShellViewId.value;
    if (exclusiveId != null) {
      final rootNav = Navigator.maybeOf(context, rootNavigator: true);
      final isOnRootNav = route?.navigator == rootNav;
      // In-shell workspace tabs: only the active surface's viewId stays up.
      // Root-cover overlays (true dialogs) are unaffected.
      if (!isOnRootNav && _viewId != exclusiveId) return false;
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
    final shouldBeVisible = computeShouldBeVisible();
    if (overlayNativeVisible != shouldBeVisible) {
      wkzTrace(
        _viewId,
        'reevaluate -> ${shouldBeVisible ? "show" : "hide"} '
        '(rootPopupOpen=${WebKitGtkOverlayHooks.isAnyRootPopupOpen} '
        'popups=${WebKitGtkOverlayHooks.rootPopupCount.value})',
      );
      overlayNativeVisible = shouldBeVisible;
      setNativeVisibility(shouldBeVisible);
      // Offstage→visible (compose tab): drop stale geometry so the first
      // setBounds cannot reuse warm-create / wrong-X coords over the inbox.
      if (shouldBeVisible) {
        lastSentGeometry = null;
        lastAckedGeometry = null;
        pendingMeasurementSize = null;
        pendingMeasurementOrigin = null;
        stableMeasurementFrames = 0;
        WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
      }
    }
    if (shouldBeVisible) {
      scheduleNativeBoundsSync();
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
      WebKitGtkOverlayHooks.exclusiveShellViewId.removeListener(
        _modalScopeListener!,
      );
      _modalScopeListener = null;
    }
    disposeBoundsSync();
    overlayNativeVisible = false;
    setNativeVisibility(false);
    _disposeWebView();
    super.dispose();
  }

  @override
  void deactivate() {
    // When this subtree is moved offstage or replaced by a different flow
    // (e.g. settings), ensure native overlay is hidden immediately.
    overlayNativeVisible = false;
    setNativeVisibility(false);
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
    resetBoundsMeasurement(forceImmediate: true);
    _reevaluateVisibility();
    scheduleNativeBoundsSync();
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
      case 'onLoadResourceWithCustomScheme':
        return resolveCustomSchemeMethodCall(
          call,
          viewHandler: widget.onLoadResourceWithCustomScheme,
        );
      case 'onMessage':
        final name = call.arguments['name'] as String;
        final payload = call.arguments['payload'];
        return widget.onMessage(name, payload);
      case 'onHostLayoutChanged':
        lastSentGeometry = null;
        lastAckedGeometry = null;
        WebKitGtkOverlayHooks.forceImmediateBoundsSync = true;
        _reevaluateVisibility();
        scheduleNativeBoundsSync();
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
        scheduleNativeBoundsSync();
        return;
      }

      final result = await WebKitGtkChannelDispatcher.channel.invokeMethod(
        'create',
        {
          'viewId': _viewId,
          if (widget.initialSettings != null) 'settings': widget.initialSettings,
          if (widget.initialUserScripts != null)
            'userScripts': widget.initialUserScripts!
                .map((script) => script.toMap())
                .toList(),
        },
      );
      WebKitGtkOverlayHooks.reportNativeHealth(
        WebKitGtkNativeHealth.fromChannel(result),
      );

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
      scheduleNativeBoundsSync();
    } catch (e) {
      final health = _healthFromCreateError(e);
      WebKitGtkOverlayHooks.reportNativeHealth(health);
      setState(() => _loadError = health.toDiagnosticText());
      widget.onLoadError('', -1, 'Failed to initialize WebView: ${health.reason ?? e}');
    }
  }

  WebKitGtkNativeHealth _healthFromCreateError(Object error) {
    if (error is PlatformException) {
      return WebKitGtkNativeHealth.fromChannel({
        ...?_asStringKeyedMap(error.details),
        'loaded': false,
        'reason': error.details is Map
            ? (_asStringKeyedMap(error.details)?['reason'] ?? error.code)
            : error.code,
        'lastError': error.message,
      });
    }
    return WebKitGtkNativeHealth(
      loaded: false,
      embedding: 'none',
      gdkBackend: 'unknown',
      webkitVersion: '',
      gtkVersion: '',
      soupMajor: '',
      webkitApi: 'webkit2gtk-4.1',
      viewId: _viewId,
      reason: error.toString(),
    );
  }

  Map<String, Object?>? _asStringKeyedMap(Object? raw) {
    if (raw is! Map) return null;
    return {
      for (final entry in raw.entries) '${entry.key}': entry.value,
    };
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

  @override
  Future<void> setNativeVisibility(bool visible) async {
    if (!_isInitialized) return;
    if (_controller == null) return;
    if (!visible) {
      lastSentGeometry = null;
    }
    wkzTrace(_viewId, 'native ${visible ? "SHOW" : "HIDE"}');
    try {
      await WebKitGtkChannelDispatcher.channel.invokeMethod(visible ? 'show' : 'hide', {
        'viewId': _viewId,
      });
    } catch (e) {
      final previous = WebKitGtkOverlayHooks.lastNativeHealth;
      WebKitGtkOverlayHooks.reportNativeHealth(
        WebKitGtkNativeHealth(
          loaded: previous?.loaded ?? true,
          embedding: previous?.embedding ?? 'unknown',
          gdkBackend: previous?.gdkBackend ?? 'unknown',
          webkitVersion: previous?.webkitVersion ?? '',
          gtkVersion: previous?.gtkVersion ?? '',
          soupMajor: previous?.soupMajor ?? '',
          webkitApi: previous?.webkitApi ?? 'webkit2gtk-4.1',
          viewId: _viewId,
          warning: 'visibility_sync_failed',
          lastError: e.toString(),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    // Keep native WebKit visible only while this route is the top-most route.
    // Flutter dialogs/popups are rendered above the route in Flutter's overlay,
    // but native GTK surfaces always paint above Flutter. Hiding native WebKit
    // during popup routes prevents the WebKit surface from covering dialogs.
    final shouldBeVisible = computeShouldBeVisible();
    if (overlayNativeVisible != shouldBeVisible) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) return;
        final next = computeShouldBeVisible();
        if (overlayNativeVisible == next) return;
        wkzTrace(_viewId, 'build -> ${next ? "show" : "hide"}');
        overlayNativeVisible = next;
        setNativeVisibility(overlayNativeVisible);
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
        if (lastLayoutConstraints == null ||
            lastLayoutConstraints != nextConstraints) {
          lastLayoutConstraints = nextConstraints;
          lastSentGeometry = null;
          lastAckedGeometry = null;
          scheduleNativeBoundsSync();
        }

        // After layout: catch sidebar/Sentria moves where constraints are
        // unchanged for a frame but the placeholder origin shifted.
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!mounted || !overlayNativeVisible) return;
          resyncIfPlaceholderMoved();
        });

        return MouseRegion(
          onEnter: (_) => syncNativeWindowPosition(bypassDebounce: true),
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
}
