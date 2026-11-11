# ScreenMirror for OPUS

A plugin that shows the screen of an original Teenage Engineering **OP-1** on your computer, including the OPUS 2 menu. By [severusnare](https://github.com/alanmunozlopez).

ScreenMirror exists thanks to [**OPUS**](https://github.com/LizardMan245/opus) by **LizardMan**. Its core, its `.opmod` plugin system and its SDK make it possible to load this plugin without reflashing the firmware. This is an independent project, not affiliated with OPUS or Teenage Engineering.

## Versions

The latest packaged release is **0.14.1** ([releases/0.14.1](releases/0.14.1/screenmirror.opmod)), with the `fast`, `balanced` and `quality` profiles. `main` contains **0.17.0** (experimental, not yet packaged in `releases/`). Build it with `sh scripts/build.sh`.

### 0.17.0: sharp only

0.17.0 has a single image mode, `sharp`: RGB332 at the native panel resolution (320×160, 376×188 or 400×200), with no scaling and no dropped pixels. The plugin ID is `severusnare.screenmirror`; earlier versions used `alan.screenmirror`. Settings saved by older versions are not carried over.

| OPUS setting | Values | Effect |
|---|---|---|
| `screen mirror` | **`on`** (default), `off` | `off` stops capture and sending: the plugin sends no MIDI at all, not even telemetry. Useful when the OP-1 is connected to a DAW as a controller. Turning it back `on` starts a new session from a fresh capture. |
| `send rate` | `x1`–`x4` (default **x4**) | Messages per main pass. `x1` keeps the 0.14 pace (one every ≥5 ms). |

Without USB, or with `screen mirror` set to `off`, the plugin does not capture the screen: the display thread only chains the original flush. The pixel memory is still reserved at start-up.

0.16.0 also offered `fast`, `balanced` and `quality`. They were removed in 0.17.0 after hardware testing:
- `fast` and `balanced` scale without filtering, which distorts 1-pixel strokes.
- `quality` (RGB565) takes about 1 s per full screen and does not work in Chrome.

With a single engine, the package went from 22,728 to 12,684 bytes, static memory from 180,240 to 143,560 bytes and the pixel pool from 640,000 to 560,008 bytes.

`sharp` needs current viewers: the desktop app built from `main` and the current web viewer. The bundled desktop app 0.14.0 rejects the 376/400 geometries in v4.

### Measured on the instrument

Measured on an OP-1 with a 400×200 panel on **10 November 2026**, with 0.15.0/0.16.0 and an input-only SysEx recorder on the computer. The removed profiles are shown for reference.

| Profile and rate | Full screen (median) | Regional change (median) |
|---|---|---|
| `balanced` x1 | 492 ms | 160 ms |
| `balanced` x4 | 54–57 ms | 40–48 ms |
| `sharp` x4 | 80 ms (p90 168 ms) | 64 ms |
| `quality` x1 / x4 | 3.6 s / 0.98 s | — |

There were no CRC errors or lost fragments in about 110,000 messages, and no audible change in audio. These are arrival times of each batch at the computer, not key-to-image latency.

**0.17.0 with OPUS 2 beta 4.1 (build 7), 11 November 2026.** The plugin loads and sends from start-up, including the OPUS menu and the top layer of the OPUS monitor (`monitor.opmod`). A 6-minute recording gave the following results:
- 28,374 messages with no CRC errors.
- With `screen mirror` set to `off`, sending and capture stopped.
- After two USB cable disconnects, the image came back by itself.

Figures with the screen changing continuously:

| Measure | Value |
|---|---|
| OPUS monitor: audio / worst block / late blocks | 9.6 % / 21.0 % / 0 |
| OPUS monitor: display thread / main thread (total, all plugins) | 35.4 % / 37.7 % |
| Plugin telemetry: capture | ~8 per second, 8.8 ms median → ~7 % of the display thread |
| Plugin telemetry: preparing each image | ~7 per second, ~20 ms → ~14 % of the main thread (USB calls not included) |

The OPUS monitor has no row of its own for ScreenMirror. Capture happens in the final-flush hook and sending in the main loop, outside the per-plugin measurements, so the plugin's figures come from its own telemetry.

Regional updates, lossless compression and dropping stale work reduce what is sent. A full-screen change takes longer; 30 FPS is not promised.

## The three projects

- **This repository:** the Blackfin plugin, sender tests and `.opmod` packages.
- [screenmirror-desktop](https://github.com/alanmunozlopez/screenmirror-desktop): native **macOS Apple Silicon** app. It decodes every profile. Windows is not implemented yet.
- [screenmirror-web](https://github.com/alanmunozlopez/screenmirror-web): Chrome viewer using Web MIDI, live at [opus-screenmirror.severusnare.com](https://opus-screenmirror.severusnare.com). It supports `fast`, `balanced` and `sharp`, but not `quality`.

## Install the plugin

It requires an OP-1 **OG with OPUS 2 installed**, based on firmware #246, with the manifest's imports available. It is not a plugin for the OP-1 field or for Hundekraft. The capture uses a #246-specific hook, checked byte by byte; it does not assume a universal framebuffer API.

1. Download the `.opmod` (`releases/0.14.1/screenmirror.opmod`, or a 0.17.0 build). Keep a copy of your previous plugin off the OP-1 disk.
2. Put the instrument in disk mode. **Once macOS shows the disk**, replace only `mods/screenmirror.opmod`. Keep a single copy of the plugin.
3. Eject the disk and restart the OP-1 as OPUS describes for plugins. The plugin starts by itself when OPUS loads it.
4. Open the desktop or web viewer. It connects to the available OP-1; Chrome asks for Web MIDI/SysEx permission the first time. Nothing needs to be configured on the instrument for streaming.

To reach the settings: **SYNTH + DRUM + TAPE + MIXER → T2 → select screenmirror with the blue encoder → press blue**. In 0.17.0 the menu has SCREEN MIRROR and SEND RATE (in 0.14.1, MIRROR MODE). Changes apply without a restart. OPUS saves a value after two seconds without changes and uses it at the next start-up.

To go back, restore your previous `.opmod` and its viewer. [0.11.0, the fast profile](releases/0.11.0/screenmirror.opmod), is also kept as the historical reference confirmed before the three modes were added. The other historical packages are described in [releases/README.md](releases/README.md). Changing the plugin never requires reflashing. If firmware recovery were ever needed, use TE-BOOT with Teenage Engineering's procedure; it is not part of this installation.

## Test without an OP-1

You need Python 3 and a C compiler with AddressSanitizer/UBSan (`cc`, Clang or GCC).

```sh
git clone https://github.com/alanmunozlopez/screenmirror-for-opus.git
cd screenmirror-for-opus
sh scripts/test-host.sh
```

This runs:
- **24 processes of the real controller and engine** under ASan/UBSan;
- codec tests against an independent Python model;
- a corpus generated with the real C engine for three panel geometries.

The minimal headers in `tests/sdk-stubs/` only provide declarations, and nothing opens a device. To repeat the C tests with the full headers of the pinned SDK:

```sh
SCREENMIRROR_TEST_SDK="$PWD/.work/opus/plugin-kit/sdk" sh scripts/test-host.sh
```

The sender corpus is written to `build/fixtures/`. The desktop repository also checks that it reconstructs those frames exactly. See [docs/VALIDATION.md](docs/VALIDATION.md).

## Build the `.opmod`

The public SDK comes from [LizardMan245/opus](https://github.com/LizardMan245/opus/tree/43b18c97ca0cde430b7d176ca4388fd5d1efeb7d/plugin-kit), pinned to commit `43b18c97ca0cde430b7d176ca4388fd5d1efeb7d`. Standard Python is enough for packaging. You also need a **bfin-elf GCC 13.3.0 / binutils 2.38** toolchain. Neither the SDK nor the compiler is included in this repository.

```sh
sh scripts/setup-sdk.sh
OP1_BFIN_TOOLCHAIN=/path/to/toolchain/install sh scripts/build.sh
```

If you already have the SDK in another checkout of that commit:

```sh
OPUS_PLUGIN_KIT=/path/to/opus/plugin-kit \
OP1_BFIN_TOOLCHAIN=/path/to/toolchain/install \
sh scripts/build.sh
```

The result is written to `build/modes/screenmirror.opmod`. It **never overwrites** the packages kept in `releases/`. `scripts/test-blackfin.sh` rebuilds the plugin and checks it byte for byte against `releases/0.14.1`, together with the assembled bridge. With the 0.17.0 sources that comparison fails by design until 0.17.0 is packaged:

```sh
OP1_BFIN_TOOLCHAIN=/path/to/toolchain/install sh scripts/test-blackfin.sh
```

On macOS Apple Silicon you can build the compiler natively with the Xcode Command Line Tools, `make`, `curl` and Homebrew:

```sh
brew install gmp mpfr libmpc
sh scripts/build-blackfin-macos.sh
sh scripts/setup-sdk.sh
sh scripts/test-blackfin.sh
```

The script downloads the GNU sources and verifies their signatures before building in `.work/`. It can take a while.
- The plugin rebuild has been checked on macOS with the toolchain already built.
- On Linux, `build.sh` uses the same `OP1_BFIN_TOOLCHAIN` interface and Python, but building a new Linux toolchain has not been repeated since the repositories were split.

See also the [pinned SDK's instructions](https://github.com/LizardMan245/opus/blob/43b18c97ca0cde430b7d176ca4388fd5d1efeb7d/plugin-kit/docs/getting-started.md).

No script installs plugins, opens physical MIDI or flashes firmware.

## Status and limits

- **Tested on the instrument:**
  - Image reception with **0.14.1 on 7 November 2026**.
  - 0.15.0–0.17.0 on 10 and 11 November 2026, including OPUS build 7 (see above).
- **Run locally:** the synthetic tests and the packaging.
- **Not measured yet:**
  - key-to-image latency;
  - sustained CPU use over long sessions;
  - coexistence with every mod.

[Architecture and transport](docs/ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [History](docs/HISTORY.md).

0.17.0 reserves about **0.70 MB of RAM** while loaded (0.14.1: about 0.93 MB). It captures and sends while USB is configured and `screen mirror` is `on`, even with no viewer open. The code registers no audio callbacks. The sending limit is one call every 5 ms, not a delivery guarantee: the transmit API offers no ACK and no queue back-pressure.

## License

GPL-3.0; see [LICENSE](LICENSE) and [NOTICE](NOTICE). The attributions to OPUS and its SDK are kept as dependency provenance.
