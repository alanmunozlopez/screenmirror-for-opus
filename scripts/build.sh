#!/bin/sh
# Build only; never opens MIDI, mounts the OP-1, installs a plugin or flashes.
set -eu
cd "$(dirname "$0")/.."
profile=${1:-modes}
case "$profile" in modes|final240|final320|native565) ;; *) echo 'Usage: sh scripts/build.sh [modes|final240|final320|native565]' >&2; exit 2;; esac
kit=${OPUS_PLUGIN_KIT:-"$PWD/.work/opus/plugin-kit"}
pinned=43b18c97ca0cde430b7d176ca4388fd5d1efeb7d
if [ ! -f "$kit/plugin_tool.py" ]; then
  echo 'Missing SDK. Run sh scripts/setup-sdk.sh or set OPUS_PLUGIN_KIT to the pinned plugin-kit directory.' >&2
  exit 1
fi
actual=$(git -C "$kit" rev-parse HEAD)
if [ "$actual" != "$pinned" ]; then
  echo "SDK commit must be $pinned; found $actual" >&2
  exit 1
fi
OP1_BFIN_TOOLCHAIN=${OP1_BFIN_TOOLCHAIN:-"$PWD/.work/bfin-native/install"}
export OP1_BFIN_TOOLCHAIN
if [ ! -x "$OP1_BFIN_TOOLCHAIN/bin/bfin-elf-gcc" ]; then
  echo 'Set OP1_BFIN_TOOLCHAIN to a Blackfin toolchain prefix (containing bin/bfin-elf-gcc).' >&2
  exit 1
fi
mkdir -p "build/$profile"
python3 "$kit/plugin_tool.py" build "src/$profile/plugin" --out "build/$profile/screenmirror.opmod"
python3 "$kit/plugin_tool.py" show "build/$profile/screenmirror.opmod"
