# quilt-esp32

> **The 5 opcodes — laid bare on the sand, riding the air.**
> **No abstractions. No runtime. Just C, the substrate,**
> **the radio, and the wind.**

[![Substrate: quilt-vm-c](https://img.shields.io/badge/Substrate-quilt--vm--c-green.svg)](https://github.com/SuperInstance/quilt-vm-c)
[![Language: C99](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Target: ESP32-S3](https://img.shields.io/badge/Target-ESP32--S3-orange.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Mesh: ESP-NOW](https://img.shields.io/badge/Mesh-ESP--NOW-yellow.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
[![Opcodes: 5](https://img.shields.io/badge/Opcodes-5-brightgreen.svg)](#the-5-opcodes)
[![Tests: 5](https://img.shields.io/badge/Tests-5%20passing-brightgreen)](#tests)

The Quilt VM substrate (`BIND`, `LINK`, `EFFECT`, `VIEW`, `TICK`)
ported to the Espressif **ESP32** (with **ESP32-S3** as the
preferred target), using **ESP-NOW** to wire a herd of boards
into a single distributed cell-graph.

The herd of ESP32s **is** the runtime. Each board holds some
cells locally. ESP-NOW propagates `BIND`s, `LINK`s, `EFFECT`s,
and `TICK`s to neighbors. The cell-graph spans the air.

---

## The Cowboy's Maxim

> The unit of architectural foundation is the opcode, not the
> framework. The 5 opcodes host 8 polyformalisms. The
> polyformalisms are one thing in N languages. The thing is a
> function from context to value with an inverse, advanced by a
> clock. The clock is the cowboy. The cowboy is the rider.

The 5 opcodes are the size. C is the desert. The ESP32 is the
open range.

---

## The Polyformalism Canon

The 5 opcodes are a **polyformalism** — the same thing in many
forms. This repo is **Layer 1 of the polyformalism stack**,
materialized on bare metal with a radio:

- **Layer 1 (this repo)** — `quilt-esp32` — the 5 opcodes in C99, on ESP32, over ESP-NOW
- **Layer 1 (C)** — [quilt-vm-c](https://github.com/SuperInstance/quilt-vm-c) — the 5 opcodes in C, the desert
- **Layer 1 (Rust)** — [quilt-vm-rust](https://github.com/SuperInstance/quilt-vm-rust) — the 5 opcodes in safe Rust, the workshop
- **Layer 1 (TypeScript)** — [quilt-vm-typescript](https://github.com/SuperInstance/quilt-vm-typescript) — the 5 opcodes in TS, the city
- **Layer 1 (Haskell)** — [quilt-vm-haskell](https://github.com/SuperInstance/quilt-vm-haskell) — the 5 opcodes in Haskell, the cathedral
- **Layer 1 (WASM)** — [quilt-vm-wasm](https://github.com/SuperInstance/quilt-vm-wasm) — the 5 opcodes in your browser, the tent

The 5 opcodes are universal. The grammar is local. The radio
is the link. The herd is the runtime.

---

## The 5 Opcodes

| Opcode | C function | Wire code | What it does | Polyformalism |
|--------|------------|-----------|--------------|---------------|
| **BIND**   | `qvm_bind(vm, name, val, free)`   | `0` | Make a thing         | a cell, a character, a tensor |
| **LINK**   | `qvm_link(vm, a, b, type)`         | `1` | Connect two things    | a formula, an edge, a weight |
| **EFFECT** | `qvm_effect(vm, t, fwd, inv, arg)` | `2` | Change, with inverse  | paste, with undo |
| **VIEW**   | `qvm_view(vm, target, viewer)`     | `3` | Read, as a viewer     | `=A1`, a perception check |
| **TICK**   | `qvm_tick(vm, dt)`                 | `4` | Advance time          | recalculate, end the round |

Same 5 words as the host port. Same struct. Same `qvm_*`
functions. The wire codes are stable so a board flashed with
this firmware speaks the same protocol as a board flashed
with the next version. The protocol is the contract. The
contract is small.

---

## Quick Start

### 1. ESP-IDF (canonical)

```bash
# One-time: install ESP-IDF v5.x and source export.sh
git clone -b v5.1.2 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3 && . ./export.sh

# Build the firmware
cd /workspace/quilt-esp32
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

The default `sdkconfig.defaults` targets the ESP32-S3. To
flash the original ESP32, run `idf.py set-target esp32`
before `build`.

### 2. PlatformIO

```bash
# Install PlatformIO (once)
pip install platformio

# Build & flash
cd /workspace/quilt-esp32
pio run -e esp32s3 -t upload
```

### 3. Host unit tests

The 5 unit tests do not need any ESP32 hardware — they
exercise the substrate directly. Build with `gcc`:

```bash
cd /workspace/quilt-esp32
gcc -std=c99 -Wall -Wextra -O2 -Iinclude \
    tests/test_vm.c src/quilt_vm.c -o test_vm
./test_vm
```

You should see 5 `PASS` lines march across the terminal — one
per opcode. **BIND. LINK. EFFECT. VIEW. TICK.** The 5 opcodes
are the size.

---

## Project Layout

```
quilt-esp32/
├── CMakeLists.txt          # ESP-IDF build
├── sdkconfig.defaults      # ESP-IDF defaults (target=esp32s3)
├── platformio.ini          # PlatformIO alternative
├── README.md               # this file
├── include/
│   ├── quilt_vm.h          # 5-opcode public API
│   └── esp_now_mesh.h      # mesh public API
├── src/
│   ├── quilt_vm.c          # port of the 5 opcodes for ESP32
│   ├── esp_now_mesh.c      # ESP-NOW peer-to-peer mesh
│   └── main.c              # entry: init WiFi, init ESP-NOW,
│                           #       init VM, register callbacks,
│                           #       run tick loop
└── tests/
    └── test_vm.c           # 5 unit tests (one per opcode)
```

`quilt_vm.c` is a **direct port** of
[`/workspace/quilt-vm-c/src/quilt_vm.c`](../quilt-vm-c/src/quilt_vm.c).
Same struct, same functions, same semantics. The 5 opcodes are
the runtime; the desert is the desert.

`esp_now_mesh.c` is the only file that knows it's an ESP32.
It is wrapped in `#include` guards for every ESP-IDF header;
`quilt_vm.c` has no ESP-specific types.

---

## The Wire Protocol

A single packed struct, max 250 bytes (one ESP-NOW frame):

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────────────────┐
│ opcode   │ reserved │ name_len │ b_len    │ type_len │ name\0 b\0 type\0 ... │
│ uint8_t  │ uint8_t  │ uint16_t │ uint16_t │ uint16_t │ variable             │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────────────────┘
```

| Wire code | Opcode | Carries | Transport |
|-----------|--------|---------|-----------|
| `0` | BIND   | `name`              | broadcast |
| `1` | LINK   | `a, b, type`        | unicast to peer owning `b`, fallback broadcast |
| `2` | EFFECT | `target`            | broadcast (forward/inverse live locally) |
| `3` | VIEW   | `target, viewer`    | broadcast (the skeleton ignores) |
| `4` | TICK   | `dt` (as ASCII)     | broadcast every N ms |

Frames are encrypted with a **herd-wide shared key** (16
bytes, all zero = no crypto) at the ESP-NOW radio layer. Set
the key in `src/main.c`:

```c
static const uint8_t HERD_KEY[16] = {
    /* fill with a 16-byte shared secret in production */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};
```

The skeleton ships with all zeros so you can flash two boards
and have them talk immediately. For a real deployment, fill
the key with a shared secret so only the herd can speak.

---

## What `main.c` Does

1. **VM init** — `qvm_new()`, then register three demo cells:
   - `bathy:0` — the cowboy's sounding (a `double`)
   - `tide:current` — the water's state (a string)
   - `viewpoint` — where the lookout faces (a string)
2. **Link init** — `bathy:0 --depends_on--> tide:current`,
   `viewpoint --observes--> bathy:0`
3. **Effect init** — register `inc`/`dec` on `bathy:0`
4. **WiFi + ESP-NOW init** — `qvm_mesh_init()` with the herd
   key. Registers send/recv callbacks.
5. **Tasks**:
   - `qvm_tick` task — runs `qvm_tick(1.0)` and broadcasts a
     TICK every 1000ms.
   - `qvm_anno` task — re-broadcasts our BINDs/LINKs every
     5000ms so new boards joining the herd see the existing
     cells.

When a peer BIND arrives, the recv callback auto-creates the
cell in the local VM. When a peer LINK arrives, the recv
callback auto-creates the edge. **The cell-graph spans the
air.**

---

## The 8 Polyformalisms, On Hardware

A real deployment would map the 8 polyformalisms onto the
herd like this:

| Polyformalism | On the ESP32 |
|---------------|--------------|
| 1. Quilt cell | `bathy:0` as a `double` |
| 2. Cordis plugin | `logger:0`, `config:main` as cells |
| 3. Spreadsheet | `A1`, `A2`, `B1` with `depends_on` links |
| 4. MUD | `room:1` as a string cell |
| 5. TTRPG | `player:gandalf` with `perception` value |
| 6. Bay dance | `boat:0`, `bay` as cells, `in` links |
| 7. The cowboy | `model:PHI-4` as a cell |
| 8. The bus | `qvm_subscribe` for tick events |

The 5 opcodes host all 8. The substrate is universal; the
grammar is local; the radio is the link.

---

## Tests

The 5 unit tests, one per opcode:

1. **`test_bind`** — BIND puts a value, qvm_find returns the
   thing.
2. **`test_link`** — LINK writes a forward and a reverse arrow.
3. **`test_effect`** — EFFECT queues the forward; TICK runs it;
   DISPOSE runs the inverse.
4. **`test_view`** — VIEW reads the value as a viewer; a
   non-existent cell returns NULL.
5. **`test_tick`** — TICK advances time, drains pending
   effects, fires subscribers.

```bash
./test_vm
# Running C tests for the 5-opcode Quilt VM (ESP32 port):
#   PASS test_bind (BIND)
#   PASS test_link (LINK)
#   PASS test_effect (EFFECT)
#   PASS test_view (VIEW)
#   PASS test_tick (TICK)
# All 5 tests passed!
# The cowboy rides. The 5 opcodes host everything.
```

---

## Performance

Same 5 opcodes as the host port — the substrate is unchanged.
On the ESP32 at 240MHz, the per-op cost is dominated by the
WiFi/ESP-NOW radio layer; the VM itself is **a few hundred
nanoseconds** per opcode. The gold demo (all 8 polyformalisms)
runs in well under a millisecond, even with the radio on.

The radio is the bottleneck. The substrate is not.

---

## Style

- **C99** portable. No C11/C17 features. The same style as
  `/workspace/quilt-vm-c/`.
- `quilt_vm.c` has **no ESP-specific types** — the mesh
  layer above is the only file that knows it's an ESP32.
- Same 5 opcodes, same struct, same API. The wire protocol
  is the contract.
- The cowboy rides. The 5 opcodes host everything. The
  composition is the value.

## Latest: the 2026-08-26 milestone

A `.qm` rule table was compiled to C at build time, vendored
into the same `quilt-vm-c` runtime, and flashed to an
**ESP32-S3** on 2026-08-26. **1Hz LED blink. No cloud. No
model. No WiFi.** RAM 6.5%, flash 20.4%, ~2.7s rebuilds.

The seam held: the first spike attempt died with
`model-required` because the table→model boundary was
unconfigured. The failure was kept, not hidden. The doctrine
is enforced at compile time, the seam is enforced at runtime,
the equivalence is enforced at test time.

The equivalence gate: the C serve path answers identically to
the Rust `qm-runner` on all 5 fixture signals. Two
implementations, one truth, both of them ours.

Read the full story:
[`docs/MILESTONE-2026-08-26.md`](docs/MILESTONE-2026-08-26.md)
and [Paper 186: A $3 Sheet of Tissue](https://github.com/SuperInstance/AI-Writings/blob/master/seed-canon/papers/paper-186.md).

See the web page: [quilt-ecosystem-web/esp32/](https://github.com/SuperInstance/quilt-ecosystem-web/tree/main/esp32).

---

## License

MIT. Same as the host port.


---

## Roaming the Quilt collection

You came through the **herd**. That's one of twenty-four doors
into the same idea — the 5-opcode polyformalism. The other doors are
metaphored for different audiences (mathematicians, hardware hackers,
web developers, hardware folks, story readers), but the substrate is
the same.

**The full map of the collection:** [COLLECTION.md](https://github.com/SuperInstance/AI-Writings/blob/master/seed-canon/COLLECTION.md)

**From here, three wander-paths you might enjoy:**

1. **[quilt-vm-c](https://github.com/SuperInstance/quilt-vm-c)** — the C99 port of the same VM
2. **[quilt-foundation](https://github.com/SuperInstance/quilt-foundation)** — the foundational doc that ties the 5 opcodes together
3. **[quilt-ecosystem-demo](https://github.com/SuperInstance/quilt-ecosystem-demo)** — the 12-inch tablet demo that uses these chips

The cowboy's maxim: *The unit of foundation is the cell, not the
opcode. The 5 opcodes are the 5 messages a cell can receive. The 24
repos are the 24 doors into the same message. The cowboy is the one
who wanders.*
