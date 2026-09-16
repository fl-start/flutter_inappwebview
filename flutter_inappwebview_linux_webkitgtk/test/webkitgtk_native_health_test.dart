import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/flutter_inappwebview_linux_webkitgtk.dart';

void main() {
  test('WebKitGtkNativeHealth.fromChannel parses create snapshot', () {
    final health = WebKitGtkNativeHealth.fromChannel({
      'loaded': true,
      'embedding': 'gtk_overlay',
      'gdkBackend': 'wayland',
      'webkitVersion': '2.44.0',
      'gtkVersion': '3.24.41',
      'soupMajor': '3',
      'webkitApi': 'webkit2gtk-4.1',
      'viewId': 3,
      'hasGtkOverlayAncestor': true,
      'hasWebViewWidget': true,
      'flViewWidth': 800,
      'flViewHeight': 600,
      'flViewRealized': true,
    });
    expect(health.loaded, isTrue);
    expect(health.needsAttention, isFalse);
    expect(health.toDiagnosticText(), contains('Linux WebView: loaded'));
    expect(health.toDiagnosticText(), contains('wayland'));
  });

  test('popup fallback is flagged for diagnostics', () {
    final health = WebKitGtkNativeHealth.fromChannel({
      'loaded': true,
      'embedding': 'popup',
      'gdkBackend': 'wayland',
      'webkitVersion': '2.44.0',
      'gtkVersion': '3.24.41',
      'soupMajor': '3',
      'warning': 'missing_gtk_overlay_ancestor_popup_fallback',
    });
    expect(health.isPopupFallback, isTrue);
    expect(health.needsAttention, isTrue);
    expect(health.toDiagnosticText(), contains('warning:'));
  });

  test('invalid payload is treated as not loaded', () {
    final health = WebKitGtkNativeHealth.fromChannel(null);
    expect(health.loaded, isFalse);
    expect(health.reason, 'invalid_health_payload');
  });
}
