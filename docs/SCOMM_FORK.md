# secMail fork notes (`fl-start/flutter_inappwebview`)

This repository tracks upstream [flutter_inappwebview](https://github.com/pichillilorenzo/flutter_inappwebview) with secMail-focused performance and packaging changes.

## Platforms secMail uses

| Platform | Package | Notes |
|----------|---------|-------|
| Windows | `flutter_inappwebview_windows` | WebView2, shared `WebViewEnvironment`, keep-alive reader/compose |
| macOS | `flutter_inappwebview_macos` | WKWebView |
| iOS | `flutter_inappwebview_ios` | WKWebView |
| Android | `flutter_inappwebview_android` | Hybrid composition in secMail |
| Linux | `flutter_inappwebview_linux_webkitgtk` | WebKitGTK GtkOverlay (secMail mail viewer) |

## secMail-specific optimizations

- **Windows `evaluateJavascript`**: page-world scripts use `ICoreWebView2::ExecuteScript` instead of CDP `Runtime.evaluate`.
- **Windows user scripts**: page-world `UserScript` uses `AddScriptToExecuteOnDocumentCreated` instead of CDP `Page.addScriptToEvaluateOnNewDocument`.

## Monorepo layout

Internal packages resolve via `path:` in `flutter_inappwebview/pubspec.yaml`. Consumers on pub.dev still use version constraints; secMail pins the git monorepo with `dependency_overrides`.

## Tagging releases for secMail

When cutting a release for secMail integration:

```bash
git tag -a v6.2.0-beta.3-scomm.N -m "secMail release N"
git push origin v6.2.0-beta.3-scomm.N
```

Point secMail `dependency_overrides` at the tag instead of `master` for reproducible builds.

## Upstream sync

1. Fetch upstream tags from `pichillilorenzo/flutter_inappwebview`.
2. Rebase `master` onto the target upstream tag.
3. Re-apply secMail patches (Windows fast paths, CI, path deps).
4. Run `.github/workflows/ci.yml` locally or via GitHub Actions.
