# fl-start/flutter_inappwebview — monorepo layout

This fork is a **single Git repository** with **separate federated packages** per
platform. secMail (and other consumers) pin each package independently via
`pubspec.yaml` `git` + `path` + `ref` entries.

Repository: https://github.com/fl-start/flutter_inappwebview

## Package tiers

| Tier | Packages | When to bump `ref` |
|------|----------|-------------------|
| **Shared** | `flutter_inappwebview`, `flutter_inappwebview_platform_interface`, `dev_packages/flutter_inappwebview_internal_annotations`, `flutter_inappwebview_web` | API contract or umbrella wiring changes |
| **Linux desktop** | `flutter_inappwebview_linux_webkitgtk` | GtkOverlay, WebKitGTK, `setBounds`, focus |
| **Windows desktop** | `flutter_inappwebview_windows` | WebView2, platform view lifecycle, custom schemes |
| **macOS desktop** | `flutter_inappwebview_macos` | WKWebView desktop embedding |
| **Android mobile** | `flutter_inappwebview_android` | Android WebView / androidx.webkit |
| **iOS mobile** | `flutter_inappwebview_ios` | WKWebView on iOS |

### Not used by secMail

- `flutter_inappwebview_linux` — upstream **WPE** implementation. secMail uses
  **`flutter_inappwebview_linux_webkitgtk`** (GtkOverlay + WebKitGTK 4.1) instead.
  WebKitGTK 4.0 / libsoup 2 are unsupported.

## Consumer pinning (`tool/secmail_pins.yaml`)

Canonical pin manifest for secMail lives at [`tool/secmail_pins.yaml`](../tool/secmail_pins.yaml).
Each tier has its own `ref` (full commit SHA). Tiers may share the same SHA when
everything was tested together; bump only the tier you changed after platform QA.

Example `pubspec.yaml` override (same repo URL, different paths/refs):

```yaml
dependency_overrides:
  flutter_inappwebview_platform_interface:
    git:
      url: https://github.com/fl-start/flutter_inappwebview.git
      ref: <shared-sha>
      path: flutter_inappwebview_platform_interface
  flutter_inappwebview_windows:
    git:
      url: https://github.com/fl-start/flutter_inappwebview.git
      ref: <windows-sha>
      path: flutter_inappwebview_windows
  flutter_inappwebview_linux_webkitgtk:
    git:
      url: https://github.com/fl-start/flutter_inappwebview.git
      ref: <linux-sha>
      path: flutter_inappwebview_linux_webkitgtk
```

`platform_interface` must remain API-compatible with every platform package ref
you pin.

## PR policy (avoid Linux ↔ Windows ping-pong)

1. **Scope PRs to one platform folder** when possible (`flutter_inappwebview_windows/**`
   *or* `flutter_inappwebview_linux_webkitgtk/**`, not both).
2. Changes to `flutter_inappwebview_platform_interface` require updating **all**
   platform implementations that implement the new contract.
3. Path-scoped CI runs per platform (see `.github/workflows/*_ci.yml`).
4. Run `scripts/check_platform_scope.sh` locally before opening a cross-cutting PR.

## Local development

```bash
# Optional: melos bootstrap (all secMail-relevant packages)
dart pub global activate melos
melos bootstrap

# Or per package
cd flutter_inappwebview_linux_webkitgtk && flutter pub get && dart analyze
cd flutter_inappwebview_windows && flutter pub get && dart analyze
```

## CI workflows

| Workflow | Trigger paths | Runner |
|----------|---------------|--------|
| `linux_webkitgtk_ci.yml` | `flutter_inappwebview_linux_webkitgtk/**` | ubuntu |
| `windows_ci.yml` | `flutter_inappwebview_windows/**` | windows |
| `macos_ci.yml` | `flutter_inappwebview_macos/**` | macos |
| `android_ci.yml` | `flutter_inappwebview_android/**` | ubuntu |
| `ios_ci.yml` | `flutter_inappwebview_ios/**` | macos |
| `ci.yml` | all pushes (umbrella integration) | multi |
