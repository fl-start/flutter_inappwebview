# WebKitGTK native plugin (Linux)

## Active embedding: GTK overlay + `setBounds`

The shipped binary uses `webview_overlay_window.cc`: a `WebKitWebView` is placed as a **GtkOverlay** child above the Flutter `FlView`. Dart in `lib/core/webview/impl/webkitgtk/` reserves layout space with a transparent placeholder and syncs geometry via the `webview_webkitgtk` method channel (`create`, `setBounds`, `setVisible`).

## Unused: Flutter Platform View sources

`webview_platform_view.cc` and `webview_platform_view_factory.cc` are **not** listed in `CMakeLists.txt` (`PLUGIN_SOURCES`). They were an experiment for in-tree `FlPlatformView` embedding. Do not link them without finishing registration and z-order tests; prefer the overlay model above.

## JS bridge

At document start the plugin injects `window.flutter_inappwebview.callHandler` → `webkit.messageHandlers.<name>.postMessage`. Handlers `emailComposer` and `openExternalUrl` forward to Dart `onMessage`.
