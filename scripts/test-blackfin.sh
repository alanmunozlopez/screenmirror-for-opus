#!/bin/sh
# Compilation and static bridge ABI test, no proprietary core or hardware needed.
set -eu
cd "$(dirname "$0")/.."
sh scripts/build.sh
python3 tests/test_final_hook_abi.py
python3 - <<'VERIFY'
from pathlib import Path
import hashlib
released=Path('releases/0.17.0/screenmirror.opmod').read_bytes()
rebuilt=Path('build/modes/screenmirror.opmod').read_bytes()
if rebuilt != released:
    raise SystemExit('Rebuilt package differs from the retained 0.17.0 artifact; inspect SDK/toolchain and output before deployment.')
print('PASS: rebuilt 0.17.0 is byte-identical:',hashlib.sha256(rebuilt).hexdigest())
VERIFY
