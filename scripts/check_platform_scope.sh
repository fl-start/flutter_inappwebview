#!/usr/bin/env bash
# Warn when a PR touches multiple desktop platform native trees in one diff.
# Usage: scripts/check_platform_scope.sh [base-ref]
# Default base-ref: origin/master

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE="${1:-origin/master}"
if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
  BASE="HEAD~1"
fi

mapfile -t CHANGED < <(git diff --name-only "$BASE"...HEAD 2>/dev/null || git diff --name-only "$BASE" HEAD)

count_hits() {
  local prefix="$1"
  local n=0
  for f in "${CHANGED[@]}"; do
    [[ "$f" == "$prefix"* ]] && n=$((n + 1))
  done
  echo "$n"
}

linux_n=$(count_hits "flutter_inappwebview_linux_webkitgtk/")
windows_n=$(count_hits "flutter_inappwebview_windows/")
macos_n=$(count_hits "flutter_inappwebview_macos/")
android_n=$(count_hits "flutter_inappwebview_android/")
ios_n=$(count_hits "flutter_inappwebview_ios/")

desktop_hits=0
[[ "$linux_n" -gt 0 ]] && desktop_hits=$((desktop_hits + 1))
[[ "$windows_n" -gt 0 ]] && desktop_hits=$((desktop_hits + 1))
[[ "$macos_n" -gt 0 ]] && desktop_hits=$((desktop_hits + 1))

mobile_hits=0
[[ "$android_n" -gt 0 ]] && mobile_hits=$((mobile_hits + 1))
[[ "$ios_n" -gt 0 ]] && mobile_hits=$((mobile_hits + 1))

if [[ "$desktop_hits" -ge 2 ]]; then
  echo "WARN: This diff touches multiple desktop platform packages." >&2
  echo "  linux_webkitgtk: $linux_n files, windows: $windows_n, macos: $macos_n" >&2
  echo "  Prefer separate PRs per platform; see docs/FL_START_MONOREPO.md" >&2
  exit 1
fi

if [[ "$mobile_hits" -ge 2 ]]; then
  echo "WARN: This diff touches both Android and iOS platform packages." >&2
  echo "  android: $android_n files, ios: $ios_n files" >&2
  exit 1
fi

echo "Platform scope OK (linux=$linux_n windows=$windows_n macos=$macos_n android=$android_n ios=$ios_n)"
