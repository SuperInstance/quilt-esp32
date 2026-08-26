# The Healing Process Behind the 2026-08-26 Milestone

*How the limb-blink ESP32 milestone came to exist — the full account, failures
included. This document is the story the README's matter-of-fact section points
to. Failures are first-class content here; the negative results mapped the
edges as much as the green blink proved the center.*

---

## The claim under test

The SuperInstance doctrine holds that a cell — a small, rule-driven unit with
explicit relationships — can run without a cloud, without a model, without a
network: that the quilt substrate is cheap and local enough to be *tissue*.
The edge-benchmarks work (2026-08-26 morning) had already proven the math fast
on host hardware: **C 110ns / WASM 3.6µs per serve vs a 197ms cloud RTT**.
What remained unproven was the last inch: **does it run on a $3 chip, flashed
by a human, blinking a real LED?**

## Pass A — the honest boundary (kept, not hidden)

The first spike attempt ended with the driver dying at the first downbeat with
a `model-required` error — because the seam was unconfigured. That failure was
deliberate: *nothing shimmed, nothing faked*, pinned by a test. The boundary
between table-tissue (cost 0) and model-tissue (the seam) is the whole
architecture, and it must fail loudly when crossed unconfigured.

## Pass B — build on host first, metal second

1. **Host harness (gcc)** — `blink.qm` → `qm2c.py` code generator →
   `qm_serve.c` + vendored `quilt_vm.c`. The generator does the parsing at
   build time so the target never needs a parser. The host run printed the
   JSON serve log line by line — including a deliberate `table-miss` case
   (phase `"sleep"`) to prove the table answers *only* what it knows.
2. **Equivalence proof** — the C serve path answers identically to the Rust
   `qm-runner` on all 5 fixture signals. Two implementations, one truth.
3. **Cross-compile (PlatformIO, esp32dev)** — clean at RAM 6.5% / flash 20.4%.

## The hardware comedy — three boards, one blink

The first flash attempt taught us the naming trap: the board that enumerated
on Windows was an **ESP32-WROOM-1**, which is not a classic WROOM at all but
an **ESP32-S3** (Espressif's "-1" generation). The correctly-targeted
WROOM-32D boards refused to enumerate (power-only USB cable or missing
CP210x/CH340 driver — untested which). Two options were laid out:

- **Path A** (doctrine-pure): fix the classic-ESP32 board's USB.
- **Path B** (fastest-to-truth): retarget the firmware to the S3 that was
  already talking.

The captain chose B. The retarget took three fixes, each a lesson:

1. **First S3 build failed** — an `#ifdef` ordering bug: the RGB-LED helper
   was defined inside a guard that didn't apply. Fixed; build green in 2.3s.
2. **No `boot_app0.bin` on S3** — the classic-ESP32 merge layout includes an
   OTA boot image the S3 toolchain doesn't emit. Dropped from the merge; the
   merged image built clean (337,440 bytes).
3. **The LED changed species** — classic devkits have a plain GPIO2 LED; the
   S3 DevKitC has an addressable **WS2812 RGB** LED. The blink came out
   *green* (`neopixelWrite`, low brightness), with a documented fallback if
   a revision wires it to GPIO38 instead of 48.

## The flash (2026-08-26 13:04 AKDT)

```
esptool v5.3.1 — Connected to ESP32-S3 (QFN56, rev v0.2) on COM14
Wrote 337440 bytes (164490 compressed) at 0x0 in 3.2s
Hash of data verified. Hard resetting via RTS pin.
```

**Confirmed by the captain at 13:19: green blink at 1Hz.** A `.qm` rule table
was driving a real LED through quilt-vm-c. No cloud. No model. No WiFi. The
doctrine had a body.

## What the healing actually was

This milestone is a small instance of the fleet's standing method — the same
one that grew the cortex's gate (cell-cascade v0.4) and re-honestied the
elephant probe the same day:

1. **Fail loudly at boundaries** — `model-required`, not a silent shim.
2. **Prove equivalence before porting** — two implementations agreeing is the
   gate, not one implementation passing.
3. **Keep the negative results** — Pass A's death, the un-enumerating boards,
   the first failed S3 build: all preserved in history with causes.
4. **Retarget fast when the environment changes** — Path B didn't compromise
   the doctrine; it changed which door the truth walked through.
5. **The captain's eyes are the last verifier** — hash-verified flash plus a
   human confirming the blink. The chain of evidence ends at a person.

## The serve path, for the record

```
blink.qm ──qm2c.py──► qm_prog.h (compile-time table)
                        │
qm_serve.c + quilt_vm.c │  (vendored, byte-identical to host build)
                        ▼
        tick(phase=on|off) ──table──► {"led":1|0} ──► LED
        tick(phase=sleep) ──table-miss──► no change (honest)
```

Every serve is a JSON line on serial. The table answers only what it knows.

---

*Related: `firmware/README-SPIKE.md` (build/spike detail), the
edge-benchmarks branch in cell-cascade (the 110ns/3.6µs numbers), and the
quilt-rust `cohesion-and-fascia` docs for the cell-relationship vocabulary
this firmware is a citizen of.*
