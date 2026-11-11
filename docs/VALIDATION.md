# Validation

The tests are local: they never touch the instrument, physical USB MIDI or mounted disks. The distributed packages are kept unchanged.

## Commands

```sh
sh scripts/test-host.sh
# With the pinned SDK and a Blackfin toolchain:
OP1_BFIN_TOOLCHAIN=/path/to/toolchain/install sh scripts/test-blackfin.sh
```

- **`verify-artifacts.py`:** SHA-256 of the production sources and the five kept packages.
- **`test_modes_independent.py`:** **24 processes under ASan/UBSan** run the real 0.17.0 controller and `sharp` engine. They check:
  - no capture before USB is seen or while it is disconnected (using the capture counter in the telemetry);
  - `screen mirror` set to `off` from start-up and live: no MIDI at all, and a fresh capture in a new session after `on`;
  - the persisted, default and out-of-range `send rate`, and live `send rate` changes without a session restart;
  - that a `mode` saved by 0.16.0 and other setting names are ignored;
  - a panel geometry change (new session at the native width);
  - the reserve race, USB disconnects, the 100 ms capture limit, the CYCLES rollover and allocation failure.

  There is a single allocation of 560,008 bytes, aligned to 32 bytes and checked before any callback is registered.
- **`test_lz_codec.py`:** the real C codec is compared byte for byte with an independent Python model, covering output guards, the work budget, truncated inputs, invalid lengths and the input limit.
- **`generate-fixtures.sh`:** the real C controller and engine generate a synthetic `sharp` corpus for 320-, 376- and 400-column panels. Each panel has seven scenes, each a keyframe followed by a one-tile regional patch, all in one session: **42 publications** in total (14 per panel).
  - `SCREENMIRROR_TEST_SEND=0..3` repeats the corpus at x1–x4.
  - The `.syx` files and references are written only to `build/fixtures/`.
  - [screenmirror-desktop](https://github.com/alanmunozlopez/screenmirror-desktop) checks the exact reconstruction with its copies in `tests/fixtures/sharp/`, and keeps the 0.16.0 four-mode corpus in `tests/fixtures/modes/`.
- **`test_final_hook_abi.py`:** a static test of the real assembly. It checks the 43 restored registers, the original arguments, a linear DAG context for C, 184 bytes of stack and a single continuation to the flush. Its negative tests catch a missing restore, a missing argument area or a missing L1 reset.
- **`test-blackfin.sh`:** rebuilds the plugin from `src/modes/plugin` and requires byte equality with `releases/0.17.0/screenmirror.opmod`: ID `severusnare.screenmirror`, **12,684 bytes**, SHA-256 `28c2cca5ab590e8d74bc2c35e510aa333869b3dc3abf499e6f41852419be9c03`. The SDK reports an image of 6,996 bytes, a zero fill of 136,564 and 10 imports. The build tested on the instrument differed only in its ID (`alan.screenmirror`).
  - Earlier releases were checked the same way at the time: 0.14.1 is 20,808 bytes, SHA-256 `5148263eb1a80e9edce3c9d821eb18b9c3b7331617f0e4ce75a547ae5446e36a`.
  - The 0.16.0 build tested on the instrument (never released) is 22,728 bytes, SHA-256 `554a200dbc32cdbb0eef66a78bcff3f20ddf936ed3133b76af485bb72ceb49ab`.

The headers in `tests/sdk-stubs/` contain only the types and prototypes the mocks need. Packaging always uses the real pinned SDK; `SCREENMIRROR_TEST_SDK` also lets you repeat the C cases with its full headers.

## Checked on the hardware, and still pending

- **0.14.1, 7 November 2026:** image confirmed. Earlier iterations (regions, final-flush capture of the menu, the fast profile) were also tried on the instrument. This confirmation has a narrow scope: it is not a measured campaign of the three modes.
- **0.15.0/0.16.0, 10 November 2026:** tested on an OP-1 with a 400×200 panel: `balanced` and `quality` from x1 to x4, and `sharp` at x4. The SysEx was recorded on the computer with an input-only tool.
  - 0 CRC errors and 0 lost fragments, apart from batches cancelled on purpose by a mode change. The timings are in the README.
  - No audible change in audio at x4; this is not an audio measurement.
  - Planning a `sharp` keyframe takes 22 ms of main (median), up to 56 ms.
- **0.17.0, 11 November 2026:** tested first with OPUS build 5 and then with **OPUS 2 beta 4.1 (build 7)**, both times with the starter pack and the OPUS monitor loaded. The recording confirmed:
  - an image from start-up, with 0 CRC errors;
  - `screen mirror` set to `off`: no messages and no captures; back `on`: a new session;
  - recovery after unplugging the USB cable: the `usb_unconfigured` counter advances and capture stops.

  The OPUS monitor showed audio at 9.6 % with 0 late blocks. With the screen changing continuously, the telemetry gives capture at ~7 % of the display thread and preparation at ~14 % of the main thread. Details are in the README.

Still to measure on the device: key-to-image latency, sustained CPU use over long sessions and coexistence with every mod. Persistence is checked through mocks of the contract, not by reading the OP-1's settings back. The exact image of the installed core has not been identified either.

The placement audit of the original core needed a local `.ldr` that is not included. The `.opmod` equality and the analysis of its bridge can be reproduced from this repository with the public SDK and toolchain. Simulation tests do not replace a cost measurement on the Blackfin, nor do they guarantee that another set of mods leaves enough memory.
