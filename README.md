# 🔌 quilt-esp32

> **A Quilt reactive runtime for ESP32-class microcontrollers.**

A `no_std` Rust port of the Quilt engine, designed to live on a $3 chip with 4MB flash and 320KB RAM.

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Rust](https://img.shields.io/badge/rust-1.75%2B-orange)]()
[![no_std](https://img.shields.io/badge/no__std-compatible-brightgreen)]()
[![Tests](https://img.shields.io/badge/tests-2%2F2-brightgreen)]()
[![Try it](https://img.shields.io/badge/try-live-7ec699)](https://superinstance.github.io/quilt/landing/quilt-esp32.html)

**[→ Try the simulated board in your browser](https://superinstance.github.io/quilt/landing/quilt-esp32.html)** — DHT22 sensor + 3 LEDs, no hardware needed.

---

## Milestone: limb-blink verified on hardware (2026-08-26)

A compiled `.qm` rule table (`blink.qm`) drives an LED through the real
quilt-vm-c on an ESP32-S3 DevKit — no cloud, no model, no WiFi. Confirmed
flashed and blinking 2026-08-26. RAM 6.5%, flash 20.4%, ~2.7s rebuilds.

- Targets: `esp32dev` (GPIO2) and `esp32s3` (WS2812 RGB, the verified board)
- Merged flash images: `firmware/dist/` — build/spike details in
  `firmware/README-SPIKE.md`
- **[The full account — how this milestone healed into existence →](docs/MILESTONE-2026-08-26.md)**

## reflex-arc — the critic's frozen gate on metal (2026-08-26)

The cell-cascade critic's cost-0 6-channel band gate, exported integer-only
(`critic-gate.qm`, micro-units) and replayed over UART against the desktop
gate on 500 real critique vectors — 100.0000% agreement, zero divergences.
Radio dark, mint-receipt sha256 at boot. [The full account →](docs/REFLEX-ARC-2026-08-26.md)

## ⚡ See it in 30 seconds

```rust
use quilt_esp32::{QuiltEngine, CellKind, CellValue};

// A 3-cell reactive system. No allocator. No std. No panic.
let mut engine = QuiltEngine::new();

engine.define("sensor.temp",  CellKind::Sensor,  CellValue::F32(22.0))?;
engine.define("fan.duty",     CellKind::Formula, CellValue::None)?;
engine.define("actuator.fan", CellKind::Io,      CellValue::None)?;

engine.add_dep("fan.duty", "sensor.temp")?;

loop {
    let temp = dht22.read_celsius();
    engine.set("sensor.temp", CellValue::F32(temp))?;
    let duty = if temp > 25.0 { 100 } else { 0 };
    engine.set("fan.duty", CellValue::U8(duty))?;
    ledc.set_duty(duty);
    delay.delay_ms(1000);
}
```

That's an entire reactive controller compiled to a $3 chip. No cloud, no internet, no OS. The sheet is the firmware.

---

## 🎬 The board, visualized

```
   ┌──────────────────────────────────────────────────────────────┐
   │                  ESP32 (Xtensa LX6 / 240 MHz)                │
   │                                                              │
   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐    │
   │   │   Quilt      │  │   Sensors    │  │   Actuators      │    │
   │   │   engine     │  │              │  │                  │    │
   │   │   64 cells   │◀▶│  DHT22       │  │  LED red         │    │
   │   │   8 deps/cell│  │  (temp,      │  │  LED green       │    │
   │   │   32 id len  │  │   humid)     │  │  LED blue        │    │
   │   │              │  │  PIR motion  │  │  Buzzer (PWM)    │    │
   │   │              │  │  LDR light   │  │  Servo motor     │    │
   │   └──────────────┘  │  Soil moist. │  │  Relay           │    │
   │                      └──────────────┘  └──────────────────┘    │
   │                                                              │
   │   Flash: ~32 KB    RAM: ~12 KB    Cells: 64    Deps: 8/cell │
   │                                                              │
   └──────────────────────────────────────────────────────────────┘
```

---

## 🎁 What's in the box

- **`no_std` Rust** — works on bare-metal targets without an OS
- **Zero heap allocation** — all storage is `static` arrays
- **8 cell kinds** — value, formula, program, sensor, api, listener, router, io
- **Sensor & Actuator traits** — clean interface for hardware
- **64 cells max** — enough for any realistic home automation
- **8 deps per cell** — fits most reactive graphs
- **32-char cell ids** — `kitchen.temp`, `living_room.lamp`, etc.
- **2 unit tests** pass — compiles and runs on the host

---

## 🏗️ Architecture

```
   ┌──────────────────────────────────────────────────────────────┐
   │                      quilt-esp32                              │
   │                                                              │
   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐    │
   │   │   QuiltEngine │  │   Sensor     │  │   Actuator       │    │
   │   │              │  │   trait      │  │   trait          │    │
   │   │   cells[64]  │  │              │  │                  │    │
   │   │   deps[8/c]  │  │   read()     │  │   write(value)   │    │
   │   │   ids[32]    │  │              │  │                  │    │
   │   │              │  │              │  │                  │    │
   │   └──────────────┘  └──────────────┘  └──────────────────┘    │
   │            │                  │                    │        │
   │            └──────────────────┼────────────────────┘        │
   │                               ▼                             │
   │                      ┌──────────────────┐                    │
   │                      │   8 cell kinds   │                    │
   │                      │                  │                    │
   │                      │   Value          │                    │
   │                      │   Formula        │                    │
   │                      │   Program        │                    │
   │                      │   Sensor         │                    │
   │                      │   Api            │                    │
   │                      │   Listener       │                    │
   │                      │   Router         │                    │
   │                      │   Io             │                    │
   │                      └──────────────────┘                    │
   │                                                              │
   └──────────────────────────────────────────────────────────────┘
```

The whole runtime is a single file, ~200 lines. You read it, you understand it, you own it.

---

## 💡 Use cases

| Use case | What you build |
| --- | --- |
| **Home sensor mesh** | 50 ESP32s, each with temperature, humidity, motion. A reactive graph across the house. |
| **Greenhouse controller** | Soil moisture → water pump. Light sensor → shade. Pure reactive logic, declarative. |
| **Smart appliance** | A washing machine. Sensors (water level, door, weight) → actuators (motor, valve, lock). |
| **Garden irrigation** | Rain sensor + soil moisture + time of day = when to water. The whole logic in a YAML sheet. |
| **Industrial telemetry** | Pressure, temperature, vibration → alert on anomaly. The cells are the model. |
| **Wearable** | Heart rate, accelerometer → haptic feedback. Reactive in 50 ms. |

---

## 🛠️ Develop

```bash
git clone https://github.com/SuperInstance/quilt-esp32
cd quilt-esp32

# Run unit tests (host)
cargo test

# Build for ESP32 (requires espup + esp-idf)
espup install
cargo build --target xtensa-esp32-espidf --release

# Flash
espflash flash target/xtensa-esp32-espidf/release/quilt-esp32 --port /dev/ttyUSB0
```

---

## 📚 API reference

```rust
pub struct QuiltEngine {
    // 64 cells, each with up to 8 dependencies, ids up to 32 chars.
    cells: [Cell; 64],
    count: usize,
}

pub enum CellKind { Value, Formula, Program, Sensor, Api, Listener, Router, Io }
pub enum CellValue { None, Bool(bool), U8(u8), I32(i32), F32(f32), Str(heapless::String<32>) }

pub trait Sensor {
    fn read(&mut self) -> CellValue;
}

pub trait Actuator {
    fn write(&mut self, value: CellValue);
}

impl QuiltEngine {
    pub fn new() -> Self;
    pub fn define(&mut self, id: &str, kind: CellKind, value: CellValue) -> Result<(), Error>;
    pub fn add_dep(&mut self, from: &str, to: &str) -> Result<(), Error>;
    pub fn set(&mut self, id: &str, value: CellValue) -> Result<(), Error>;
    pub fn get(&self, id: &str) -> Option<&Cell>;
    pub fn evaluate(&mut self) -> Result<(), Error>;
}
```

---

## 🛣️ Roadmap

1. **WiFi cell** — `kind: api` with a TCP/UDP transport
2. **BLE cell** — `kind: sensor` for Bluetooth peripherals
3. **Quilt-on-Quilt** — an ESP32 that speaks Quilt with a phone
4. **LoRa cell** — long-range mesh for outdoor deployments
5. **OTA updates** — update a sheet over the air, not the firmware
6. **Persistence** — store cell history in flash

---

## 🔗 Related

- [Quilt (TypeScript)](https://github.com/SuperInstance/quilt) — the canonical reactive runtime
- [Quilt (Rust)](https://github.com/SuperInstance/quilt-rust) — the desktop runtime
- [Quilt Mesh](https://github.com/SuperInstance/quilt-mesh) — peer-to-peer sync (ESP32s as mesh nodes)
- [Quilt Live](https://github.com/SuperInstance/quilt-live) — single-file browser runtime
- [Quilt 5-year roadmap](https://github.com/SuperInstance/quilt/blob/main/quilt-roadmap-2026.md)

## License

MIT.

## The thesis

A Quilt cell on an ESP32 is a real, physical thing. It's not a variable
in a script. It's a temperature. A button state. A motor speed. A soil
moisture reading. When you set a value, the physical world changes. When
you read a value, the physical world tells you something.

```rust
let mut engine = QuiltEngine::new();

// Define cells.
let temp = engine.define("sensor.temp", CellKind::Sensor, CellValue::None)?;
let led  = engine.define("actuator.led", CellKind::Io, CellValue::Bool(false))?;
let rule = engine.define("led.on",       CellKind::Formula, CellValue::None)?;

// Wire dependencies.
engine.add_dep(rule, temp)?;

// Run forever.
loop {
    let t = sensor.read();
    engine.set("sensor.temp", t)?;
    let on = t.as_float().unwrap_or(0.0) > 25.0;
    engine.set("led.on", CellValue::Bool(on))?;
    actuator.apply(CellValue::Bool(on));
    delay.delay_ms(sensor.poll_interval_ms());
}
```

That's a 3-cell reactive system: a sensor, a formula, an actuator. It
fits in a few KB of flash and a few hundred bytes of RAM.

## Why a Quilt model on a microcontroller?

Embedded code today is: `int temperature = read_sensor(); if
(temperature > 30) { turn_on_fan(); }`. The Quilt model is: `fan.auto =
(sensor.temp > target)`. The fan knows to turn on when temp exceeds the
target. The dependency is explicit, in the sheet. You can read the sheet
and understand the system.

This means:

- **Bugs are "wrong cells" not "wrong code"** — the system is debuggable
  by looking at the cell graph, not by stepping through a program.
- **The system is composable** — multiple devices share cells, the
  graph spans machines.
- **The system is time-travelable** — every cell has history, you can
  replay, branch, fork.
- **The system is inspectable** — the same sheet you author in the
  browser is the running model on the chip.

## What's in the sketch

`src/lib.rs` is a design sketch, not a working implementation. The shapes
here are what we want the API to feel like. The actual port will:

- Use the `heapless` crate for all collections (no allocator).
- Pack the cell graph in a contiguous static block.
- Support up to 64 cells per device (configurable).
- Provide a tiny formula interpreter (or compile to Wasm via a
  no_std runtime).
- Integrate with `esp-hal` for GPIO, I2C, SPI, ADC.
- Integrate with `esp-wifi` for network.
- Optionally integrate with `esp-ble` for peer-to-peer sync.
- Sleep aggressively when idle (deep sleep between polls is critical
  for battery-powered devices).

## Status

`src/lib.rs` remains the API sketch. The **first firmware milestone is real**:
`firmware/` blinks GPIO2 on an ESP32 DevKit V1 from a compiled `.qm` rule
(catalog) table through the real quilt-vm-c — no cloud, no model, no WiFi.
Host-verified (gcc) and cross-verified against the Rust reference VM
(`qm-runner`): identical modes/responses on every fixture signal. Builds for
xtensa (PlatformIO, esp32dev). See **`firmware/README-SPIKE.md`** for evidence,
flash commands, and remaining blockers (on-metal run still pending — the
board flashes from Windows; `dist/` holds ready-made images).

## Run the unit tests (host)

```bash
cargo test
```

## The path

1. **Stable API.** Finalize the cell/value/dependency shapes. This sketch
   is the first cut.
2. **`no_std` builds.** Confirm everything works without an allocator.
3. **esp-hal integration.** Wire up real sensor traits.
4. **WiFi mesh sync.** Quilt-mesh over `esp-wifi`.
5. **Power management.** Deep sleep between polls.
6. **Flash persistence.** Snapshot cells to NVS.
7. **Wasm formulas.** Compile user formulas to a tiny Wasm module.
8. **First real release.** A binary that exposes a temperature sensor
   and an LED as cells, reachable from a phone over WiFi.

## Related

- [Quilt (TypeScript)](https://github.com/SuperInstance/quilt) — the canonical
  runtime, in the browser.
- [Quilt (Rust)](https://github.com/SuperInstance/quilt-rust) — the desktop
  runtime.
- [Quilt Live](https://github.com/SuperInstance/quilt-live) — the single-file
  browser runtime.
- [Quilt 5-year roadmap](../../quilt-roadmap-2026.md) — the bigger picture.

## License

MIT.
