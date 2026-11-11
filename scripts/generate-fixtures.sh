#!/bin/sh
# Actual controller and sharp engine; synthetic framebuffer only. No MIDI device.
set -eu
cd "$(dirname "$0")/.."
sdk=${SCREENMIRROR_TEST_SDK:-tests/sdk-stubs}
mkdir -p build/fixtures
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -I"$sdk" \
  src/modes/plugin/screenmirror.c src/modes/plugin/rgb_sender.c \
  tests/ModesFixtures.c -o build/fixtures/generator
for panel in 320 376 400; do
  rm -rf "build/fixtures/panel$panel"; mkdir -p "build/fixtures/panel$panel"
  build/fixtures/generator "$panel" "build/fixtures/panel$panel" >"build/fixtures/panel$panel/stream.syx"
done
