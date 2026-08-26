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
