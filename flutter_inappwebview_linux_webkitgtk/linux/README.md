# WebKitGTK native plugin (Linux)

## Dependencies

This plugin requires **GTK 3** plus the WebKitGTK **4.1** stack. There is no fallback to WebKitGTK 4.0, JavaScriptCoreGTK 4.0, or libsoup 2.

| Component | pkg-config |
|-----------|------------|
| GTK 3 | `gtk+-3.0` |
| WebKitGTK | `webkit2gtk-4.1 >= 2.40` |
| JavaScriptCoreGTK | `javascriptcoregtk-4.1 >= 2.40` |

2.40 is the API floor, not a security floor. Mail rendering should run the newest stable WebKitGTK; check [WebKitGTK security advisories](https://webkitgtk.org/security.html) against the `webkitVersion` reported in native health.
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

## JS bridge

At document start the plugin injects `window.flutter_inappwebview.callHandler` → `webkit.messageHandlers.<name>.postMessage`. Handlers `emailComposer` and `openExternalUrl` forward to Dart `onMessage`; the handler's return value settles the page's promise.

Every native → Dart event that belongs to one view (`onMessage`, `onLoadStart`/`Stop`/`Error`, `shouldOverrideUrlLoading`, `onLoadResourceWithCustomScheme`) carries `viewId`. When that view has no Dart handler, `WebKitGtkChannelDispatcher` refuses the event instead of handing it to another view: navigation is cancelled and a bridge message is rejected.

## Security defaults

| Control | Behavior |
|---------|----------|
| Network seal | `blockNetworkLoads` defaults to **on** (also when no settings map arrives). Blocked views get their own ephemeral context with a dead proxy, a content filter for `http(s)`/`ws(s)`, and a navigation allowlist (`about:`, `data:`, `blob:`, registered custom schemes, `file:` only with `allowFileAccess`). |
| Navigation policy | If Dart gives no usable `shouldOverrideUrlLoading` answer, the navigation is refused. |
| Web process sandbox | Enabled on every context when it can start: inside Flatpak, or with `bwrap` on `PATH` outside a snap. Reported as `webProcessSandbox` in native health. WebKit aborts if it cannot launch the sandbox, so test new targets; `WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1` is WebKit's own escape hatch. |
| Shared context | Views that are neither incognito nor network-blocked share one context. Context-wide state (cache model, cookie policy, proxy) is never set from a single view's settings. |
| Permissions | Everything is denied except geolocation with `geolocationEnabled`. `<a ping>` requests are http(s) and fall to the network seal; WebKitGTK no longer offers a setting for them. |
| Custom-scheme CORS | `Access-Control-Allow-Origin: *` is sent only to custom-scheme or opaque (`null`) origins, never to `http(s)` pages. |
