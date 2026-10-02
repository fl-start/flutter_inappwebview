# WebKitGTK native plugin (Linux)

## Dependencies

This plugin requires **GTK 3** plus the WebKitGTK **4.1** stack. There is no fallback to WebKitGTK 4.0, JavaScriptCoreGTK 4.0, or libsoup 2.

| Component | pkg-config |
|-----------|------------|
| GTK 3 | `gtk+-3.0` |
| WebKitGTK | `webkit2gtk-4.1` |
| JavaScriptCoreGTK | `javascriptcoregtk-4.1` |
| libsoup | `libsoup-3.0` |

GTK 4 and `webkitgtk-6.0` are **not** used (Flutter Linux still embeds GTK 3).

**Native / distro packages:** the host must provide the modules above at build and runtime.

**Flatpak:** the app uses the **Flatpak runtime’s** GTK/WebKitGTK/libsoup, not the host distribution’s packages. The runtime must still supply GTK 3 + WebKitGTK 4.1 + JavaScriptCoreGTK 4.1 + libsoup 3.

Example (Debian/Ubuntu):

```bash
sudo apt install \
  libgtk-3-dev \
  libwebkit2gtk-4.1-dev \
  libjavascriptcoregtk-4.1-dev \
  libsoup-3.0-dev \
  pkg-config
```

Package names differ on other distributions; use `pkg-config --exists gtk+-3.0 webkit2gtk-4.1 javascriptcoregtk-4.1 libsoup-3.0` to verify.

## Active embedding: GTK overlay + `setBounds`

The shipped binary uses `webview_overlay_window.cc`: a `WebKitWebView` is placed as a **GtkOverlay** child above the Flutter `FlView`. Dart overlay widgets reserve layout space with a transparent placeholder and sync geometry via the `webview_webkitgtk` method channel (`create`, `setBounds`, `show`/`hide`, `dispose`).

```text
GtkOverlay
├── FlView
└── WebKitWebView (per viewId)
```

### Coordinate contract

| Side | Units |
|------|--------|
| Dart `setBounds` | FlView-local **logical** pixels (`coordinateSpace: flutterLogical`) |
| Native allocation | GtkOverlay widget pixels via `flView_allocated / viewLogical` scale |
| DPR | Diagnostics / unrealized fallback only — **do not** multiply logical coords by DPR on Dart then scale again natively |

Placeholder origin: `localToGlobal(ancestor: RenderView) / devicePixelRatio`. Host `boundsProvider(viewId)` is optional and must use the same space; return `null` for non-matching views so reader and composer never share one rectangle.

Payload includes monotonic `sequence` (stale updates ignored) and edge fields for deterministic rounding.

## Unused: Flutter Platform View sources

`webview_platform_view.cc` and `webview_platform_view_factory.cc` are **not** listed in `CMakeLists.txt` (`PLUGIN_SOURCES`). They were an experiment for in-tree `FlPlatformView` embedding. Do not link them without finishing registration and z-order tests; prefer the overlay model above.

## JS bridge

At document start the plugin injects `window.flutter_inappwebview.callHandler` → `webkit.messageHandlers.<name>.postMessage`. Handlers `emailComposer` and `openExternalUrl` forward to Dart `onMessage`.
