# Capture, memory and transport

The current implementation lives in `src/modes/plugin/`. The shared folder `src/lz-regions-source/` keeps the codec's relative includes without changing any runtime code.

## Capturing the menu and normal screens

`manifest.txt` installs a `place call` at `0x019AB8C8`, expecting `07e3caaf`: firmware #246's final screen flush. Unlike `opus_on_frame`, that path includes the OPUS menu. It is neither an address in the core's menu nor a public, generic framebuffer-copy API.

The `flush_entry.S` bridge:
- preserves 43 registers;
- clears the circular registers L0–L3 before calling C;
- reserves the argument area;
- chains to the original flush exactly once.

The bridge uses 184 bytes of stack, plus whatever C needs. The ABI test inspects the assembled Blackfin object and includes negative controls; it does not run instructions on a real Blackfin CPU.

The callback validates the range, the alignment, the invalidation mode and a geometry of 320, 376 or 400 columns. It reads RGB565 with the SDK's documented layout, `buf[256*(width-1-x)+y]`, copies a snapshot and never keeps the firmware pointer.

Capture is limited to one every 100 ms. It happens only while main has seen USB configured and `screen mirror` is `on`; otherwise the callback returns without reading pixels. The display callback never allocates memory, compresses or calls USB.

## Single mode and settings

Since 0.17.0 there is a single engine, `sharp` (`rgb_sender.c`): RGB332 at the panel's native geometry. Its width comes from each capture, and a geometry change opens a new session.

The manifest offers two settings:
- `screen on,off`, labelled `screen mirror`, initially `on`;
- `send rate x1..x4`, initially x4. `send` changes `sm_burst` without restarting the session.

Any other setting name is ignored, including a `mode` saved by 0.16.0. The plugin ID is `severusnare.screenmirror` (`alan.screenmirror` up to 0.16.0).

Only main writes `capture_allowed`, and the display thread reads it:
- **Pause.** Main clears the flag when `screen` is `off` or `fw_usb_configured()` fails, and at that point drops pending work and the reference. When `off`, it sends nothing, not even telemetry.
- **Resume.** Main marks the last published snapshot as handled, so an image taken before the pause is never sent, and then allows capture again. If the pause came from `off`, it opens a new session.

The pixel pool is reserved in `opl_init` even when the setting is `off`, because OPUS only allows allocation at start-up.

`opl_init` publishes the token `(1<<2)|SM_SHARP` once, after preparing the engine. The epoch-0 token is reserved for the uninitialised state: without a valid token, the display hook does not capture. Each snapshot keeps the token it was taken with.

Up to 0.16.0 the manifest also offered `fast`, `balanced` and `quality` (the RGB565 engine `native_sender.c`), and every mode change created a new epoch and session. The sources of the earlier profiles are kept in `src/final240`, `src/final320` and `src/native565`.

## Memory and the 0.14.1 fix

Since 0.16.0 the RGB332 engine keeps its full-image buffers in the pixel pool: RLE, LZ output and LZ candidate, up to 400×200. Only main uses them.

In 0.17.0, without the RGB565 engine, the pool holds three snapshots, the reference, the RLE buffer and two LZ buffers: **560,008 bytes**. Static memory is **143,560 bytes** (6,996 of image plus 136,564 of zero fill).

LZSS4 is still limited to inputs of 65,534 bytes. At 376/400 columns, the keyframe uses LZ over the RLE stream (transform 1) or RLE/raw, with no format change.

A single `opus_alloc` in `opl_init` holds the snapshots and the reference: 640,000 bytes up to 0.16.0, and 560,008 bytes since 0.17.0. Following the OPUS contract, the memory is zeroed, aligned to 32 bytes and lives for the whole boot. If the allocation fails, `opl_init` returns -1 before registering any callback. Nothing is allocated during capture or transmission.

In 0.14.1 the compiled image holds 11,592 bytes plus 278,412 of zero fill: **290,004 bytes** of static code and data, plus the dynamic pixel pool. Total memory stays close to 930 KB. The change keeps those 640 KB out of the segment that contains the hook's code.

0.14.0 had 929,880 static bytes. In the audited local core dev66, the largest near pool was 693,856 bytes; the loader placed the plugin far away and the `CALL` was out of range before `opl_init`. The fix rebuilds identically here.

The audited local core's SHA-256 was `5394528efefcea4c2729d34f6f681dd42bb311d93fdb3b9bc2298835f935d566`; the installed core's identity was never read. This repository does not include that `.ldr`, and does not present the core analysis as a test that can run without it.

## Format and recovery

Each message is SysEx with the non-commercial ID `0x7d`, the signature `SM`, a version, a 7-bit packed payload and CRC16-CCITT. Messages carry session, sequence, geometry, fragmentation and size limits. Receivers check the CRC and the limits before publishing pixels.

- **v4 / RGB332:** 8×4 regions and a full reference. Raw, RLE or bounded LZSS is chosen by size. A dense change produces a keyframe.
- **v5 / RGB565:** independent 8-row stripes, with the exact colour of the physical capture; only the changed stripes are updated. It was used by `quality`, up to 0.16.0.
- **v3 / telemetry:** separate counters for callbacks, captures, planning, sends and interaction. They are not receipt ACKs.

The LZSS codec has:
- a 4096-byte window;
- 4 search candidates;
- matches of 3–18 bytes;
- a budget of 8192 comparisons per block of 1024 input positions.

The output has an explicit capacity; if a candidate does not fit, another representation is chosen. Compression and incremental sends happen only in main.

The newest snapshot takes priority; there is no growing queue of old images. Sends are spaced at least **5 ms from the return of the previous USB call**. Since 0.15.0, `send rate` allows up to 4 messages in a row in the same pass after that interval; x1 keeps one message per pass. The USB configuration is checked before sending. The audio transport is not touched and no audio callbacks are registered.

`fw_midi_send` returns void: a call does not prove delivery, and the queue may drop messages. There is no per-fragment ACK or retransmission. Periodic keyframes, about every 8 seconds since the last completed one, correct a lost reference. Decoders reject inconsistent packets and wait for a valid base. The protocol reduces visible errors, but it does not turn USB MIDI into a guaranteed video channel.

Closing the viewer does not stop capture or streaming while USB stays configured: that would need a signal from the receiver. To stop it, set `screen mirror` to `off`.
