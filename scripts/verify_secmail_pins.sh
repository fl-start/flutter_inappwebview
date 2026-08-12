#!/usr/bin/env bash
# Verify secMail pubspec pins match tool/secmail_pins.yaml in this repo.
# Run from flutter_inappwebview root after updating secmail_pins.yaml.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PINS="$ROOT/tool/secmail_pins.yaml"

if [[ ! -f "$PINS" ]]; then
  echo "Missing $PINS" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 required" >&2
  exit 1
fi

python3 - "$PINS" <<'PY'
import re
import sys
from pathlib import Path

pins_path = Path(sys.argv[1])
text = pins_path.read_text()
repo_m = re.search(r"^repository:\s*(\S+)", text, re.M)
if not repo_m:
    raise SystemExit("repository: missing in secmail_pins.yaml")
repo = repo_m.group(1)

# Parse tier blocks: tier_name:\n  ref: sha\n  packages:\n  - name: ...\n    path: ...
tiers = {}
current = None
for line in text.splitlines():
    m = re.match(r"^(\w+):\s*$", line)
    if m and m.group(1) not in ("packages",):
        current = m.group(1)
        if current != "repository":
            tiers[current] = {"ref": None, "packages": []}
        continue
    if current and current != "repository":
        m = re.match(r"^\s+ref:\s*(\S+)", line)
        if m:
            tiers[current]["ref"] = m.group(1)
        m = re.match(r"^\s+-\s+name:\s*(\S+)", line)
        if m:
            tiers[current]["packages"].append({"name": m.group(1)})
        m = re.match(r"^\s+path:\s*(\S+)", line)
        if m and tiers[current]["packages"]:
            tiers[current]["packages"][-1]["path"] = m.group(1)

expected = {}
for tier, data in tiers.items():
    ref = data.get("ref")
    if not ref:
        raise SystemExit(f"tier {tier} missing ref")
    for pkg in data["packages"]:
        expected[pkg["name"]] = (repo, ref, pkg["path"])

print(f"Canonical manifest: {len(expected)} packages @ {repo}")
for name in sorted(expected):
    r, ref, path = expected[name]
    print(f"  {name}: ref={ref[:12]}… path={path}")
PY

echo "Update secMail3/pubspec.yaml to mirror tool/secmail_pins.yaml (see docs/FL_START_MONOREPO.md)"
