"""Check the Qt application's build record against the configured source checkout."""
import json
from pathlib import Path
import subprocess
import sys

exe, version, channel = sys.argv[1:]
root = Path(__file__).resolve().parents[1]
data = json.loads(subprocess.check_output([exe, "--build-info"], timeout=30))
assert version == (root / "VERSION").read_text().strip() == data["version"]
assert channel == data["channel"] and data["architecture"]
if (root / ".git").exists():
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    assert head == data["commit"]
