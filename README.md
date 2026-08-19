# quilt-esp32

> A Quilt reactive runtime for ESP32-class microcontrollers.

A `no_std` Rust port of the Quilt engine, designed to live on a $3 chip
with 4MB flash and 320KB RAM.

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

Sketch only. No working binary yet. The shapes here are the API design
target.

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
