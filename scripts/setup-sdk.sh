#!/bin/sh
# Downloads public source into ignored .work/. Never builds or installs firmware.
set -eu
cd "$(dirname "$0")/.."
commit=43b18c97ca0cde430b7d176ca4388fd5d1efeb7d
target="$PWD/.work/opus"
mkdir -p .work
if [ ! -d "$target" ]; then
  git clone --no-checkout https://github.com/LizardMan245/opus.git "$target"
  git -C "$target" checkout --detach "$commit"
fi
actual=$(git -C "$target" rev-parse HEAD)
if [ "$actual" != "$commit" ]; then
  echo "Existing SDK is $actual, expected $commit. Not modifying it." >&2
  exit 1
fi
printf '%s\n' "Pinned SDK: $target/plugin-kit ($commit)"
