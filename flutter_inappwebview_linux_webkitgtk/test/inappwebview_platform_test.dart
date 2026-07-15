import 'package:flutter/foundation.dart';
import 'package:flutter_inappwebview_linux_webkitgtk/flutter_inappwebview_linux_webkitgtk.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('registerWith installs the WebKitGTK platform implementation', () {
    LinuxWebKitGtkInAppWebViewPlatform.registerWith();

    expect(
      InAppWebViewPlatform.instance,
      isA<LinuxWebKitGtkInAppWebViewPlatform>(),
    );
  });

  test('platform creates WebKitGTK widget and controller implementations', () {
    final platform = LinuxWebKitGtkInAppWebViewPlatform();

    expect(
      platform.createPlatformInAppWebViewWidget(
        PlatformInAppWebViewWidgetCreationParams(),
      ),
      isA<LinuxWebKitGtkInAppWebViewWidget>(),
    );
    expect(
      platform.createPlatformInAppWebViewController(
        const PlatformInAppWebViewControllerCreationParams(id: 42),
      ),
      isA<LinuxWebKitGtkInAppWebViewController>(),
    );
  });

  test('capability metadata reports implemented JavaScript handlers', () {
    final controller = LinuxWebKitGtkInAppWebViewController.static();
    expect(
      controller.isMethodSupported(
        PlatformInAppWebViewControllerMethod.addJavaScriptHandler,
        platform: TargetPlatform.linux,
      ),
      isTrue,
    );
    expect(
      controller.isMethodSupported(
        PlatformInAppWebViewControllerMethod.removeJavaScriptHandler,
        platform: TargetPlatform.linux,
      ),
      isTrue,
    );
  });
}
