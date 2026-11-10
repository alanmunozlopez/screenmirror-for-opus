#!/usr/bin/env python3
"""Verify retained source/release bytes; never opens a device."""
from pathlib import Path
import hashlib
import json
root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'ARTIFACTS.json').read_text())
for name, expected in manifest['files'].items():
    actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f'Checksum mismatch: {name}: {actual}')
print(f"PASS: {len(manifest['files'])} source/release checksums")
