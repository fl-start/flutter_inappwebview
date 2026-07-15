import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

import 'in_app_webview/in_app_webview.dart';
import 'in_app_webview/in_app_webview_controller.dart';
import 'linux_auxiliary_stubs/cookie_manager/cookie_manager.dart';
import 'linux_auxiliary_stubs/find_interaction/find_interaction_controller.dart';
import 'linux_auxiliary_stubs/http_auth_credentials_database.dart';
import 'linux_auxiliary_stubs/proxy_controller/proxy_controller.dart';
import 'linux_auxiliary_stubs/web_message/web_message_channel.dart';
import 'linux_auxiliary_stubs/web_message/web_message_listener.dart';
import 'linux_auxiliary_stubs/web_message/web_message_port.dart';
import 'linux_auxiliary_stubs/web_storage/web_storage.dart';
import 'linux_auxiliary_stubs/web_storage/web_storage_manager.dart';
import 'linux_auxiliary_stubs/webview_environment/webview_environment.dart';

/// WebKitGTK GtkOverlay implementation of [InAppWebViewPlatform] for secMail.
class LinuxWebKitGtkInAppWebViewPlatform extends InAppWebViewPlatform {
  static void registerWith() {
    InAppWebViewPlatform.instance = LinuxWebKitGtkInAppWebViewPlatform();
  }

  @override
  LinuxWebKitGtkInAppWebViewController createPlatformInAppWebViewController(
    PlatformInAppWebViewControllerCreationParams params,
  ) {
    return LinuxWebKitGtkInAppWebViewController(params);
  }

  @override
  LinuxWebKitGtkInAppWebViewController createPlatformInAppWebViewControllerStatic() {
    return LinuxWebKitGtkInAppWebViewController.static();
  }

  @override
  LinuxWebKitGtkInAppWebViewWidget createPlatformInAppWebViewWidget(
    PlatformInAppWebViewWidgetCreationParams params,
  ) {
    return LinuxWebKitGtkInAppWebViewWidget(params);
  }

  @override
  LinuxWebKitGtkInAppWebViewWidget createPlatformInAppWebViewWidgetStatic() {
    return LinuxWebKitGtkInAppWebViewWidget.static();
  }

  @override
  PlatformCookieManager createPlatformCookieManagerStatic() =>
      LinuxCookieManager.static();

  @override
  LinuxCookieManager createPlatformCookieManager(
    PlatformCookieManagerCreationParams params,
  ) =>
      LinuxCookieManager(params);

  @override
  PlatformWebViewEnvironment createPlatformWebViewEnvironmentStatic() =>
      LinuxWebViewEnvironment.static();

  @override
  LinuxWebViewEnvironment createPlatformWebViewEnvironment(
    PlatformWebViewEnvironmentCreationParams params,
  ) =>
      LinuxWebViewEnvironment(params);

  @override
  PlatformChromeSafariBrowser createPlatformChromeSafariBrowserStatic() =>
      _PlatformChromeSafariBrowser.static();

  @override
  LinuxHttpAuthCredentialDatabase createPlatformHttpAuthCredentialDatabase(
    PlatformHttpAuthCredentialDatabaseCreationParams params,
  ) =>
      LinuxHttpAuthCredentialDatabase(params);

  @override
  LinuxHttpAuthCredentialDatabase
      createPlatformHttpAuthCredentialDatabaseStatic() =>
          LinuxHttpAuthCredentialDatabase.static();

  @override
  PlatformInAppBrowser createPlatformInAppBrowserStatic() =>
      _PlatformInAppBrowser.static();

  @override
  PlatformInAppBrowser createPlatformInAppBrowser(
    PlatformInAppBrowserCreationParams params,
  ) =>
      _PlatformInAppBrowser(params);

  @override
  PlatformHeadlessInAppWebView createPlatformHeadlessInAppWebViewStatic() =>
      _PlatformHeadlessInAppWebView.static();

  @override
  PlatformHeadlessInAppWebView createPlatformHeadlessInAppWebView(
    PlatformHeadlessInAppWebViewCreationParams params,
  ) =>
      _PlatformHeadlessInAppWebView(params);

  @override
  PlatformProcessGlobalConfig createPlatformProcessGlobalConfigStatic() =>
      _PlatformProcessGlobalConfig.static();

  @override
  PlatformProxyController createPlatformProxyControllerStatic() =>
      LinuxProxyController.static();

  @override
  LinuxProxyController createPlatformProxyController(
    PlatformProxyControllerCreationParams params,
  ) =>
      LinuxProxyController(params);

  @override
  PlatformServiceWorkerController
      createPlatformServiceWorkerControllerStatic() =>
          _PlatformServiceWorkerController.static();

  @override
  PlatformTracingController createPlatformTracingControllerStatic() =>
      _PlatformTracingController.static();

  @override
  PlatformFindInteractionController
      createPlatformFindInteractionControllerStatic() =>
          LinuxFindInteractionController.static();

  @override
  LinuxFindInteractionController createPlatformFindInteractionController(
    PlatformFindInteractionControllerCreationParams params,
  ) =>
      LinuxFindInteractionController(params);

  @override
  PlatformPrintJobController createPlatformPrintJobControllerStatic() =>
      _PlatformPrintJobController.static();

  @override
  PlatformPullToRefreshController
      createPlatformPullToRefreshControllerStatic() =>
          _PlatformPullToRefreshController.static();

  @override
  PlatformWebAuthenticationSession
      createPlatformWebAuthenticationSessionStatic() =>
          _PlatformWebAuthenticationSession.static();

  @override
  PlatformWebNotificationController
      createPlatformWebNotificationControllerStatic() =>
          _PlatformWebNotificationController.static();

  @override
  LinuxWebMessageChannel createPlatformWebMessageChannelStatic() =>
      LinuxWebMessageChannel.static();

  @override
  LinuxWebMessageChannel createPlatformWebMessageChannel(
    PlatformWebMessageChannelCreationParams params,
  ) =>
      LinuxWebMessageChannel(params);

  @override
  LinuxWebMessagePort createPlatformWebMessagePort(
    PlatformWebMessagePortCreationParams params,
  ) =>
      LinuxWebMessagePort(params);

  @override
  PlatformWebMessageListener createPlatformWebMessageListenerStatic() =>
      LinuxWebMessageListener.static();

  @override
  LinuxWebMessageListener createPlatformWebMessageListener(
    PlatformWebMessageListenerCreationParams params,
  ) =>
      LinuxWebMessageListener(params);

  @override
  LinuxWebStorage createPlatformWebStorage(
    PlatformWebStorageCreationParams params,
  ) =>
      LinuxWebStorage(params);

  @override
  LinuxWebStorage createPlatformWebStorageStatic() => LinuxWebStorage(
        LinuxWebStorageCreationParams(
          localStorage: createPlatformLocalStorageStatic(),
          sessionStorage: createPlatformSessionStorageStatic(),
        ),
      );

  @override
  LinuxLocalStorage createPlatformLocalStorage(
    PlatformLocalStorageCreationParams params,
  ) =>
      LinuxLocalStorage(params);

  @override
  LinuxLocalStorage createPlatformLocalStorageStatic() =>
      LinuxLocalStorage.defaultStorage(controller: null);

  @override
  LinuxSessionStorage createPlatformSessionStorage(
    PlatformSessionStorageCreationParams params,
  ) =>
      LinuxSessionStorage(params);

  @override
  LinuxSessionStorage createPlatformSessionStorageStatic() =>
      LinuxSessionStorage.defaultStorage(controller: null);

  @override
  PlatformWebStorageManager createPlatformWebStorageManagerStatic() =>
      LinuxWebStorageManager.static();

  @override
  LinuxWebStorageManager createPlatformWebStorageManager(
    PlatformWebStorageManagerCreationParams params,
  ) =>
      LinuxWebStorageManager(params);

  @override
  PlatformAssetsPathHandler createPlatformAssetsPathHandlerStatic() =>
      _PlatformAssetsPathHandler.static();

  @override
  PlatformResourcesPathHandler createPlatformResourcesPathHandlerStatic() =>
      _PlatformResourcesPathHandler.static();

  @override
  PlatformInternalStoragePathHandler
      createPlatformInternalStoragePathHandlerStatic() =>
          _PlatformInternalStoragePathHandler.static();

  @override
  PlatformCustomPathHandler createPlatformCustomPathHandlerStatic() =>
      _PlatformCustomPathHandler.static();

  @override
  DefaultInAppLocalhostServer createPlatformInAppLocalhostServer(
    PlatformInAppLocalhostServerCreationParams params,
  ) =>
      DefaultInAppLocalhostServer(params);

  @override
  DefaultInAppLocalhostServer createPlatformInAppLocalhostServerStatic() =>
      DefaultInAppLocalhostServer.static();

  @override
  PlatformWebViewFeature createPlatformWebViewFeatureStatic() =>
      _PlatformWebViewFeature.static();
}

class _PlatformInAppBrowser extends PlatformInAppBrowser {
  _PlatformInAppBrowser(PlatformInAppBrowserCreationParams params)
      : super.implementation(params);
  static final _PlatformInAppBrowser _static = _PlatformInAppBrowser(
    PlatformInAppBrowserCreationParams(),
  );
  factory _PlatformInAppBrowser.static() => _static;
}

class _PlatformHeadlessInAppWebView extends PlatformHeadlessInAppWebView {
  _PlatformHeadlessInAppWebView(
    PlatformHeadlessInAppWebViewCreationParams params,
  ) : super.implementation(params);
  static final _PlatformHeadlessInAppWebView _static =
      _PlatformHeadlessInAppWebView(
        const PlatformHeadlessInAppWebViewCreationParams(),
      );
  factory _PlatformHeadlessInAppWebView.static() => _static;
}

class _PlatformChromeSafariBrowser extends PlatformChromeSafariBrowser {
  _PlatformChromeSafariBrowser(PlatformChromeSafariBrowserCreationParams params)
      : super.implementation(params);
  static final _PlatformChromeSafariBrowser _static =
      _PlatformChromeSafariBrowser(
        const PlatformChromeSafariBrowserCreationParams(),
      );
  factory _PlatformChromeSafariBrowser.static() => _static;
}

class _PlatformProcessGlobalConfig extends PlatformProcessGlobalConfig {
  _PlatformProcessGlobalConfig(PlatformProcessGlobalConfigCreationParams params)
      : super.implementation(params);
  static final _PlatformProcessGlobalConfig _static =
      _PlatformProcessGlobalConfig(
        const PlatformProcessGlobalConfigCreationParams(),
      );
  factory _PlatformProcessGlobalConfig.static() => _static;
}

class _PlatformServiceWorkerController extends PlatformServiceWorkerController {
  _PlatformServiceWorkerController(
    PlatformServiceWorkerControllerCreationParams params,
  ) : super.implementation(params);
  static final _PlatformServiceWorkerController _static =
      _PlatformServiceWorkerController(
        const PlatformServiceWorkerControllerCreationParams(),
      );
  factory _PlatformServiceWorkerController.static() => _static;
  @override
  ServiceWorkerClient? get serviceWorkerClient => null;
}

class _PlatformTracingController extends PlatformTracingController {
  _PlatformTracingController(PlatformTracingControllerCreationParams params)
      : super.implementation(params);
  static final _PlatformTracingController _static =
      _PlatformTracingController(
        const PlatformTracingControllerCreationParams(),
      );
  factory _PlatformTracingController.static() => _static;
}

class _PlatformPrintJobController extends PlatformPrintJobController {
  _PlatformPrintJobController(PlatformPrintJobControllerCreationParams params)
      : super.implementation(params);
  static final _PlatformPrintJobController _static =
      _PlatformPrintJobController(
        const PlatformPrintJobControllerCreationParams(id: ''),
      );
  factory _PlatformPrintJobController.static() => _static;
}

class _PlatformPullToRefreshController extends PlatformPullToRefreshController {
  _PlatformPullToRefreshController(
    PlatformPullToRefreshControllerCreationParams params,
  ) : super.implementation(params);
  static final _PlatformPullToRefreshController _static =
      _PlatformPullToRefreshController(
        PlatformPullToRefreshControllerCreationParams(),
      );
  factory _PlatformPullToRefreshController.static() => _static;
}

class _PlatformWebAuthenticationSession
    extends PlatformWebAuthenticationSession {
  _PlatformWebAuthenticationSession(
    PlatformWebAuthenticationSessionCreationParams params,
  ) : super.implementation(params);
  static final _PlatformWebAuthenticationSession _static =
      _PlatformWebAuthenticationSession(
        const PlatformWebAuthenticationSessionCreationParams(),
      );
  factory _PlatformWebAuthenticationSession.static() => _static;
}

class _PlatformWebNotificationController
    extends PlatformWebNotificationController {
  _PlatformWebNotificationController(
    PlatformWebNotificationControllerCreationParams params,
  ) : super.implementation(params);
  static final _PlatformWebNotificationController _static =
      _PlatformWebNotificationController(
        PlatformWebNotificationControllerCreationParams(
          id: '',
          notification: WebNotification(),
        ),
      );
  factory _PlatformWebNotificationController.static() => _static;
}

class _PlatformWebViewFeature extends PlatformWebViewFeature {
  _PlatformWebViewFeature(PlatformWebViewFeatureCreationParams params)
      : super.implementation(params);
  static final _PlatformWebViewFeature _static = _PlatformWebViewFeature(
    PlatformWebViewFeatureCreationParams(),
  );
  factory _PlatformWebViewFeature.static() => _static;
}

class _PlatformAssetsPathHandler extends PlatformAssetsPathHandler {
  _PlatformAssetsPathHandler(PlatformAssetsPathHandlerCreationParams params)
      : super.implementation(params);
  static final _PlatformAssetsPathHandler _static = _PlatformAssetsPathHandler(
    PlatformAssetsPathHandlerCreationParams(
      PlatformPathHandlerCreationParams(path: ''),
    ),
  );
  factory _PlatformAssetsPathHandler.static() => _static;
  @override
  PlatformPathHandlerEvents? eventHandler;
  @override
  Map<String, dynamic> toMap({EnumMethod? enumMethod}) => {
    'path': path,
    'type': type,
  };
  @override
  Map<String, dynamic> toJson() => toMap();
}

class _PlatformResourcesPathHandler extends PlatformResourcesPathHandler {
  _PlatformResourcesPathHandler(
    PlatformResourcesPathHandlerCreationParams params,
  ) : super.implementation(params);
  static final _PlatformResourcesPathHandler _static =
      _PlatformResourcesPathHandler(
        PlatformResourcesPathHandlerCreationParams(
          PlatformPathHandlerCreationParams(path: ''),
        ),
      );
  factory _PlatformResourcesPathHandler.static() => _static;
  @override
  PlatformPathHandlerEvents? eventHandler;
  @override
  Map<String, dynamic> toMap({EnumMethod? enumMethod}) => {
    'path': path,
    'type': type,
  };
  @override
  Map<String, dynamic> toJson() => toMap();
}

class _PlatformInternalStoragePathHandler
    extends PlatformInternalStoragePathHandler {
  _PlatformInternalStoragePathHandler(
    PlatformInternalStoragePathHandlerCreationParams params,
  ) : super.implementation(params);
  static final _PlatformInternalStoragePathHandler _static =
      _PlatformInternalStoragePathHandler(
        PlatformInternalStoragePathHandlerCreationParams(
          PlatformPathHandlerCreationParams(path: ''),
          directory: '',
        ),
      );
  factory _PlatformInternalStoragePathHandler.static() => _static;
  @override
  PlatformPathHandlerEvents? eventHandler;
  @override
  Map<String, dynamic> toMap({EnumMethod? enumMethod}) => {
    'path': path,
    'type': type,
  };
  @override
  Map<String, dynamic> toJson() => toMap();
}

class _PlatformCustomPathHandler extends PlatformCustomPathHandler {
  _PlatformCustomPathHandler(PlatformCustomPathHandlerCreationParams params)
      : super.implementation(params);
  static final _PlatformCustomPathHandler _static = _PlatformCustomPathHandler(
    PlatformCustomPathHandlerCreationParams(
      PlatformPathHandlerCreationParams(path: ''),
    ),
  );
  factory _PlatformCustomPathHandler.static() => _static;
  @override
  PlatformPathHandlerEvents? eventHandler;
  @override
  Map<String, dynamic> toMap({EnumMethod? enumMethod}) => {
    'path': path,
    'type': type,
  };
  @override
  Map<String, dynamic> toJson() => toMap();
}
