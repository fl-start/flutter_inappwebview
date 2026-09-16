/// Snapshot of native WebKitGTK overlay create/load state.
class WebKitGtkNativeHealth {
  const WebKitGtkNativeHealth({
    required this.loaded,
    required this.embedding,
    required this.gdkBackend,
    required this.webkitVersion,
    required this.gtkVersion,
    required this.soupMajor,
    required this.webkitApi,
    this.viewId,
    this.hasGtkOverlayAncestor,
    this.hasWebViewWidget,
    this.flViewWidth,
    this.flViewHeight,
    this.flViewRealized,
    this.overlayCount,
    this.webkitDisableCompositing,
    this.reason,
    this.warning,
    this.lastError,
  });

  final bool loaded;
  final String embedding;
  final String gdkBackend;
  final String webkitVersion;
  final String gtkVersion;
  final String soupMajor;
  final String webkitApi;
  final int? viewId;
  final bool? hasGtkOverlayAncestor;
  final bool? hasWebViewWidget;
  final int? flViewWidth;
  final int? flViewHeight;
  final bool? flViewRealized;
  final int? overlayCount;
  final String? webkitDisableCompositing;
  final String? reason;
  final String? warning;
  final String? lastError;

  bool get isPopupFallback =>
      embedding == 'popup' ||
      warning == 'missing_gtk_overlay_ancestor_popup_fallback' ||
      lastError == 'missing_gtk_overlay_ancestor_popup_fallback';

  bool get needsAttention => !loaded || isPopupFallback || (reason != null && reason!.isNotEmpty);

  factory WebKitGtkNativeHealth.fromChannel(Object? raw) {
    if (raw is! Map) {
      return const WebKitGtkNativeHealth(
        loaded: false,
        embedding: 'none',
        gdkBackend: 'unknown',
        webkitVersion: '',
        gtkVersion: '',
        soupMajor: '',
        webkitApi: 'webkit2gtk-4.1',
        reason: 'invalid_health_payload',
      );
    }
    final map = <String, Object?>{};
    raw.forEach((key, value) {
      map['$key'] = value;
    });
    bool? asBool(Object? v) {
      if (v is bool) return v;
      return null;
    }

    int? asInt(Object? v) {
      if (v is int) return v;
      if (v is num) return v.toInt();
      return null;
    }

    String asString(Object? v, [String fallback = '']) {
      if (v == null) return fallback;
      return '$v';
    }

    return WebKitGtkNativeHealth(
      loaded: asBool(map['loaded']) ?? false,
      embedding: asString(map['embedding'], 'none'),
      gdkBackend: asString(map['gdkBackend'], 'unknown'),
      webkitVersion: asString(map['webkitVersion']),
      gtkVersion: asString(map['gtkVersion']),
      soupMajor: asString(map['soupMajor']),
      webkitApi: asString(map['webkitApi'], 'webkit2gtk-4.1'),
      viewId: asInt(map['viewId']),
      hasGtkOverlayAncestor: asBool(map['hasGtkOverlayAncestor']),
      hasWebViewWidget: asBool(map['hasWebViewWidget']),
      flViewWidth: asInt(map['flViewWidth']),
      flViewHeight: asInt(map['flViewHeight']),
      flViewRealized: asBool(map['flViewRealized']),
      overlayCount: asInt(map['overlayCount']),
      webkitDisableCompositing: map['webkitDisableCompositing'] == null
          ? null
          : asString(map['webkitDisableCompositing']),
      reason: map['reason'] == null ? null : asString(map['reason']),
      warning: map['warning'] == null ? null : asString(map['warning']),
      lastError: map['lastError'] == null ? null : asString(map['lastError']),
    );
  }

  Map<String, Object?> toMap() => {
        'loaded': loaded,
        'embedding': embedding,
        'gdkBackend': gdkBackend,
        'webkitVersion': webkitVersion,
        'gtkVersion': gtkVersion,
        'soupMajor': soupMajor,
        'webkitApi': webkitApi,
        if (viewId != null) 'viewId': viewId,
        if (hasGtkOverlayAncestor != null)
          'hasGtkOverlayAncestor': hasGtkOverlayAncestor,
        if (hasWebViewWidget != null) 'hasWebViewWidget': hasWebViewWidget,
        if (flViewWidth != null) 'flViewWidth': flViewWidth,
        if (flViewHeight != null) 'flViewHeight': flViewHeight,
        if (flViewRealized != null) 'flViewRealized': flViewRealized,
        if (overlayCount != null) 'overlayCount': overlayCount,
        if (webkitDisableCompositing != null)
          'webkitDisableCompositing': webkitDisableCompositing,
        if (reason != null) 'reason': reason,
        if (warning != null) 'warning': warning,
        if (lastError != null) 'lastError': lastError,
      };

  String toDiagnosticText() {
    final lines = <String>[
      'Linux WebView: ${loaded ? 'loaded' : 'NOT loaded'}',
      'embedding: $embedding',
      'gdk: $gdkBackend',
      'webkit: $webkitVersion ($webkitApi)',
      'gtk: $gtkVersion  libsoup: $soupMajor',
    ];
    if (viewId != null) lines.add('viewId: $viewId');
    if (overlayCount != null) lines.add('overlays: $overlayCount');
    if (flViewWidth != null) {
      lines.add(
        'FlView: ${flViewWidth}x$flViewHeight realized=${flViewRealized ?? false}',
      );
    }
    if (webkitDisableCompositing != null) {
      lines.add('WEBKIT_DISABLE_COMPOSITING_MODE: $webkitDisableCompositing');
    }
    if (reason != null && reason!.isNotEmpty) lines.add('reason: $reason');
    if (warning != null && warning!.isNotEmpty) lines.add('warning: $warning');
    if (lastError != null && lastError!.isNotEmpty) {
      lines.add('lastError: $lastError');
    }
    return lines.join('\n');
  }
}
