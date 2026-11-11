# History and provenance

This repository separates the plugin from the web and macOS viewers. The original workspace, its five commits and its `main` and `improve/menu-legibility` branches are kept elsewhere; that history is not needed to build here. The reference development snapshot was `e2e07295011a2d3d7a0d84b4500f41e082d6f69e`. The C, H and S files and the manifests of the kept profiles keep their original bytes.

| Version | Change and observation |
|---|---|
| 0.3–0.6 | First mono/colour captures at reduced resolutions; sending a full image was slow. |
| 0.7 | Gray120 as the responsiveness reference. A page change in Drums showed up before small changes in Tape. |
| 0.8 | Regional updates in gray120; felt smoother. |
| 0.9 | 160×80 colour with regions; limited legibility, responsiveness at the edge of comfortable. |
| 0.10 | Bounded LZSS compression and regional transport; the 240×120 variant was the best experience at the time. |
| 0.11 | Capture at the final flush before the firmware: includes the OPUS menu that `opus_on_frame` did not deliver. Fast 240×120 reference kept. |
| 0.12 | RGB332 at 320×160: more legible menu and text, longer wait on full changes. |
| 0.13 | RGB565 at native resolution, independent stripes, HQ viewer. |
| 0.14.0 | One plugin with a fast/balanced/quality choice, fast by default. It failed to load on the audited local core because of too much near BSS. |
| 0.14.1 | Pixels reserved once with `opus_alloc`. Package of 20,808 bytes; static memory down to 290,004 bytes. Image confirmed on the instrument on 7 November 2026. |
| 0.15.0 (experimental) | `send rate` setting x1–x4, and RGB332 capture in blocks of 8 columns (from ~20 ms down to 5–14 ms on the display thread). Measured on the instrument on 10 November 2026, from x1 to x4 and without losses: `balanced` went from 492 to 54 ms per full screen, `quality` from 3.6 to 0.98 s. |
| 0.16.0 (experimental) | `sharp` profile: unscaled RGB332 at native resolution, default together with x4. Compression buffers moved into the `opus_alloc` pool; static memory 180,240 bytes (12,640 plus 167,600 zero fill). Confirmed on the instrument on 10 November 2026: 80 ms median per full screen, legible text, no audible change in audio. |
| 0.17.0 | Only `sharp` remains: `fast`, `balanced` and `quality` (the RGB565 v5 engine) and the `mode` setting are removed. New `screen mirror` on/off setting; no capture without USB or when `off`. Plugin ID renamed to `severusnare.screenmirror`. Package from 22,728 to 12,684 bytes; static memory 143,560 bytes (6,996 plus 136,564 zero fill); pixel pool from 640,000 to 560,008 bytes. No protocol change; telemetry reports 0.17.0. Tested on the instrument with OPUS build 5 and build 7 (beta 4.1) on 11 November 2026. |

The packages 0.11.0, 0.12.0 and 0.13.0 are kept, with their sources, for reference and compatible recovery. The faulty 0.14.0 package is not included. The historical smoothness ratings are the author's subjective impressions, not latency measurements. The bundled desktop app has its own version, 0.14.0, and works with plugin 0.14.1.
