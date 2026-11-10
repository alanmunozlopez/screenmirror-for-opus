#!/bin/sh
# Synthetic host-only tests. No Blackfin SDK, MIDI, USB device or firmware needed.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
python3 scripts/verify-artifacts.py
python3 tests/test_modes_independent.py
python3 tests/test_lz_codec.py
sh scripts/generate-fixtures.sh
printf '%s\n' 'PASS: host controller/engines, bounded codec and synthetic corpus generation.'
