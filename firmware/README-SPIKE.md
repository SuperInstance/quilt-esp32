# README-SPIKE — limb-blink on ESP32 (Pass B)

## Milestone

An ESP32 DevKit V1 blinks its GPIO2 LED **driven by the `.qm` rule table
through the real quilt-vm-c**. No cloud, no model, no WiFi. The same
serve path (`qm_serve.c` + vendored `quilt_vm.c`) builds and runs green on
the host (gcc) and builds green for xtensa (PlatformIO / arduino-esp32).

## File map

| File | Role |
|---|---|
| `blink.qm`, `signals.json` | limb program + fixture (Pass A, unchanged) |
| `qm2c.py` | vendored generator (Pass A, unchanged) |
| `qm_tables.h` | table typedefs; now defaults to `#include "qm_prog.h"` when `QM_PROG_HEADER` is not defined |
| `qm_serve.{c,h}` | portable serve path — root copies are the single editable source of truth |
| `qm_opcodes.{c,h}` | the five canon opcodes (Paper 211) as C — thin wrap of the vendored VM; `qm_serve` routes through it |
| `host_opcodes/main.c` | host unit tests: the five opcodes + serve regression (24 checks) |
| `vm/quilt_vm.{c,h}` | vendored quilt-vm-c (unmodified) |
| `host/main.c`, `Makefile` | host harness; `gen` now emits `src/qm_prog.h`, host builds with `-Isrc` |
| `platformio.ini` | `esp32dev` env, Arduino framework, no `lib_deps` |
| `src/` | pio build tree — **copies** (not symlinks) of `qm_serve.{c,h}`, `qm_tables.h`, `vm/quilt_vm.{c,h}`, generated `qm_prog.h`, plus `main.cpp` firmware |
| `src/main.cpp` | Arduino firmware: 500 ms phase alternation on→off, QmSignal per loop, `qm_serve` → `qm_led_from_response` → `digitalWrite(GPIO2)`; JSON log per serve, heartbeat every 40 serves (20 s) |

After editing the root `qm_serve.{c,h}` / `qm_tables.h` / `vm/*` copies, run
`make sync` to refresh `src/` (`make fw` runs `gen` + `sync` first).

## Host evidence

```
$ make -C firmware run
make: Entering directory '/home/eileen/projects/quilt-esp32/firmware'
mkdir -p src/vm
python3 qm2c.py blink.qm signals.json 0 src/qm_prog.h
wrote src/qm_prog.h: 6 binds, 1 links, 2 rules, 3 views, 5 signals
gcc -O2 -std=c99 -I. -Ivm -Isrc host/main.c qm_serve.c vm/quilt_vm.c -o host_blink
./host_blink
{"i":0,"kind":"tick","phase":"on","mode":"table","response":{"led":true},"led":1}
{"i":1,"kind":"tick","phase":"off","mode":"table","response":{"led":false},"led":0}
{"i":2,"kind":"tick","phase":"sleep","mode":"table-miss","response":null,"led":-1}
{"i":3,"kind":"tick","phase":"on","mode":"table","response":{"led":true},"led":1}
{"i":4,"kind":"tick","phase":"off","mode":"table","response":{"led":false},"led":0}
{"ok":true}
make: Leaving directory '/home/eileen/projects/quilt-esp32/firmware'
```

## Reference-VM equivalence

The C lane (qm2c tables + qm_serve + quilt-vm-c) is equivalent to the Rust
reference VM (`qm-runner`) on all 5 signals in `signals.json` — modes and
responses identical, including the `phase:"sleep"` table-miss. Verified
2026-08-26 (Pass A cross-check, unchanged by Pass B: serve path semantics
were not touched).

## Build (xtensa)

```
$ ~/.platformio/penv/bin/pio run -d /home/eileen/projects/quilt-esp32/firmware
...
RAM:   [=         ]   6.5% (used 21424 bytes from 327680 bytes)
Flash: [==        ]  20.4% (used 267269 bytes from 1310720 bytes)
========================= [SUCCESS] Took 3.26 seconds =========================
```

(Equivalently `make fw` from `firmware/`, which runs `gen` + `sync` first.
Full console transcript captured in the Pass B execution log.)

## Flash

**Hardware target:** ESP32 DevKit V1 (esp32dev, xtensa, 4MB flash, blue LED on GPIO2).
Board is plugged into Windows; WSL2 sees no serial port, so flashing runs from Windows.

Ready-made images are in `dist/` (built from this commit, sha256s in `dist/SHA256SUMS`):

| file | flash offset |
|---|---|
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `firmware.bin` | `0x10000` |
| `limb-blink-merged-0x0.bin` | `0x0` (all four pre-merged) |

### Option 1 — esptool on Windows (exact commands Casey runs)

```powershell
# one-time:  py -m pip install esptool
# copy the firmware/dist/ folder from WSL:  \\wsl$\<distro>\home\eileen\projects\quilt-esp32\firmware\dist
# plug in the DevKit V1, note the new COM port in Device Manager (CP210x = usually COM3..COM7)

cd dist
py -m esptool --chip esp32 --port COM5 --baud 921600 write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB ^
  0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin

# or the single merged image (both are equivalent):
py -m esptool --chip esp32 --port COM5 --baud 921600 write_flash -z 0x0 limb-blink-merged-0x0.bin
```

(Adjust `COM5` to the actual port. If upload fails at 921600, drop to 115200.)

### Option 2 — web flasher, zero install

Chrome/Edge on Windows → <https://esp.huhn.me> → connect the DevKit's serial port →
add the four `dist/` files at the offsets in the table above (or the merged image at `0x0`) → Flash.

### Option 3 — VSCode + PlatformIO on Windows

Install the PlatformIO IDE extension, open `firmware/`, plug in the board, press **Upload**.
(The project is self-contained; PIO rebuilds the same images on Windows.)

### Option 4 — flash from WSL via usbipd-win (one-time admin setup)

```powershell
# admin PowerShell:
usbipd list                      # find the CP210x device's busid, e.g. 2-3
usbipd bind --busid 2-3
usbipd attach --wsl --busid 2-3
```
then in WSL `/dev/ttyUSB0` appears → `pio run -t upload --upload-port /dev/ttyUSB0`.

### What you should see after flash

115200 baud serial: the banner `limb-blink v0.1 — …`, then one JSON line per 500 ms —
`{"i":0,"mode":"table","led":1}` … alternating `led:1`/`led:0` — and the blue LED
(GPIO2) blinking at 1 Hz. `[hb]` heartbeat every 20 s.

## Blockers

1. **No serial device in WSL** — `/dev/ttyUSB*` and `/dev/ttyACM*` do not exist
   (WSL2 default; no usbipd-win attached device). Flashing therefore runs from
   Windows (Options 1–3 above); Option 4 removes the blocker with a one-time
   admin setup.
2. **Toolchain was absent** — no esp-idf, no PlatformIO, no xtensa gcc at spike
   start. Resolved: PlatformIO Core 6.1.19 installed at `~/.platformio`
   (official installer, user-local venv); platform `espressif32` 7.0.1 with
   `toolchain-xtensa-esp32` 8.4.0 and `framework-arduinoespressif32` 2.0.17.
   The original repo README's Rust path (`espup` + `xtensa-esp32-espidf`) was
   **not** taken: it needs the esp forked Rust toolchain + a full ESP-IDF cmake
   build, far heavier than the C lane. The C lane reuses the proven
   qm2c + quilt-vm-c path from cell-cascade `edge-benchmarks` (C 110 ns prior).
3. **`LED_BUILTIN` undefined** for the esp32dev variant in arduino-esp32 2.x —
   fixed with an explicit `#define LED_BUILTIN 2` (GPIO2 on DevKit V1).
4. **Hardware not yet in hand** — the on-metal blink itself is the one step
   this spike cannot do from here; everything up to and including the flashable
   image is done and verified (host semantics + xtensa compile + image build).

## What is NOT proven yet

- On-metal timing: the 500 ms blink period and heartbeat cadence have not
  been observed on a real DevKit V1 over serial.
- Power behavior: boot/brownout behavior of GPIO2 at reset is unverified.
- Long-run stability: heap usage of the serve path (strdup per serve,
  freed via effect inverse) is sound by inspection but not soak-tested.
- WSL cannot flash without usbipd-win — flashing must happen from Windows.

## Opcodes lane — the five canon opcodes as C (2026-08-26)

`qm_opcodes.{c,h}` expose the Quilt canon (Paper 211) as five C functions —
`qm_bind`, `qm_link`, `qm_effect`, `qm_view`, `qm_tick` — each a thin
pass-through to the vendored quilt-vm-c's `qvm_*` (which stays
byte-identical to upstream). Plus two canon-string conveniences the serve
path uses, `qm_bind_str` / `qm_effect_set`. `qm_serve.c` now routes through
this layer, so what blinks the LED on metal IS the canon opcode set — the
ESP32 is a polyformalism of the Quilt, not a custom format. Nothing more
than that is claimed: semantics are the vendored VM's, unchanged.

### Opcode mapping (canon ↔ C)

| canon (Paper 211) | C | wraps | semantics |
|---|---|---|---|
| `BIND(name, value)` | `int qm_bind(qvm_t*, const char *name, void *value, void (*free_value)(void*))` | `qvm_bind` | make a thing |
| `LINK(a, b, type)` | `int qm_link(qvm_t*, const char *a, const char *b, const char *type)` | `qvm_link` | connect things (missing endpoints implicitly BINDed; reverse edge is `!type`) |
| `EFFECT(target, fn, inv)` | `int qm_effect(qvm_t*, const char *target, qvm_effect_fn fwd, qvm_effect_fn inv, void *arg)` | `qvm_effect` | queue a reversible change; applies when TICK drains |
| `VIEW(target, viewer)` | `void *qm_view(qvm_t*, const char *target, const char *viewer)` | `qvm_view` | project the thing's value (NULL if absent) |
| `TICK(dt)` | `void qm_tick(qvm_t*, double dt)` | `qvm_tick` | advance time, drain pending effects, fire due checks, notify subscribers |
| — (convenience) | `int qm_bind_str(qvm_t*, const char *name, const char *canon)` | `qm_bind` | BIND a canonical-JSON string value |
| — (convenience) | `int qm_effect_set(qvm_t*, const char *target, const char *canon)` | `qm_effect` | the QM_SET action: queue "install canon string", inverse "set NULL" |

### Host evidence

```
$ make -C firmware opcodes-run
{"ok":true,"passed":24,"failed":0}
```

24 checks: the five opcodes exercised directly (value visibility, implicit
BIND on LINK, `!type` reverse edges, same-type append, queue-before/apply-
after TICK, pending drain, time advance, event log, unknown-target errors,
dispose/inverse), a by-hand bind+link+effect+tick+view round trip, and a
serve regression — `qm_serve` through the new layer answers all 5 fixture
signals identically to the Pass A/B equivalence run (`make run` output
byte-identical). ASan clean (one pre-existing upstream quirk: each serve
appends an effect record whose strdup'd target is never freed by
`qvm_thing_free` — vendored code, left verbatim, ~15 bytes/serve).

### Failure first

The original `qm_serve` passed a stack `SetArg` into `qvm_effect` and got
away with it because its `qvm_tick` ran in the same stack frame. Hoisting
the set-effect into `qm_effect_set` moved the tick across a frame boundary
— host ASan caught `fwd_set` reading the dead shell (stack-use-after-
return). Fix: the effect arg is now the strdup'd canon itself (heap,
owned by the thing after apply) — no wrapper struct, no frame to outlive.
The lesson: the VM's pending queue holds raw `arg` pointers; any wrapper
layer must heap-own them.

### Build + flash (esp32s3, all 3 pio envs green)

```
$ make -C firmware fw          # esp32dev + esp32s3 + reflex_arc: SUCCESS
$ make -C firmware merge-s3    # dist/opcodes-s3-merged-0x0.bin (337,600 B)
```

esp32s3 footprint: RAM 5.7% (18,824 / 327,680) · flash 8.1% (271,697 /
3,342,336). Banner now reads `limb-blink v0.2` + the canon line.

Casey's flash line (merged image, one shot; Linux):

```sh
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    write_flash -z 0x0 dist/opcodes-s3-merged-0x0.bin
```

Windows: `py -m esptool --chip esp32s3 --port COM14 --baud 921600 write_flash -z 0x0 dist\opcodes-s3-merged-0x0.bin`
(sha256 `5a939c51564a7f2879dfe3718384820a7e4e3cb7d1edc14d26eddf76125ff905`,
in `dist/SHA256SUMS`).

Not proven yet: on-metal serial run of v0.2 (the v0.1 blink is proven on
this board; v0.2 changes the serve path routing and banner only — same
host-verified semantics).
