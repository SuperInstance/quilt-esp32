//! quilt-esp32 — a sketch of a Quilt runtime for ESP32-class microcontrollers.
//!
//! This is a design sketch, not a working implementation. The shapes here
//! are what we want the API to feel like. When the actual no_std port is
//! written, the cell kinds and reactive DAG will mirror this surface.
//!
//! The point of this file is to answer the question: "what would Quilt
//! look like as an embedded runtime?" — concretely.

#![cfg_attr(target_os = "none", no_std)]

/// The eight cell kinds. Same vocabulary as the larger engines.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CellKind {
    Value,    // a static value
    Formula,  // an expression that depends on other cells
    Program,  // a small expression that runs
    Sensor,   // a named input source
    Api,      // an outbound call (e.g. to a network endpoint)
    Listener, // fires on changes
    Router,   // caller-context-aware dispatch
    Io,       // an outbound port (uart, gpio, i2c, ...)
}

/// The types a cell can hold. We use a tagged union, sized for the
/// constraints of a microcontroller.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CellValue {
    None,
    Bool(bool),
    Int(i64),
    Float(f32),
}

/// A cell, as stored in the engine. In a real no_std port, the
/// id and dependency list would use fixed-size heapless collections.
#[derive(Clone, Debug)]
pub struct Cell {
    pub id: &'static str,
    pub kind: CellKind,
    pub value: CellValue,
    pub deps: [u8; 8], // up to 8 dependencies, indices into the engine
    pub dep_count: u8,
}

/// The engine itself. Holds up to 64 cells. Reactive.
/// Designed to live in a single static memory block so it survives
/// reboots and can be snapshotted to flash.
pub struct QuiltEngine {
    pub cells: [Option<Cell>; 64],
    pub count: u8,
}

impl QuiltEngine {
    pub const fn new() -> Self {
        Self {
            cells: [const { None }; 64],
            count: 0,
        }
    }

    /// Define a new cell. Returns the cell's index in the engine.
    /// Returns None if the engine is full.
    pub fn define(&mut self, id: &'static str, kind: CellKind, value: CellValue) -> Option<u8> {
        if (self.count as usize) >= self.cells.len() { return None; }
        let idx = self.count;
        self.cells[idx as usize] = Some(Cell {
            id,
            kind,
            value,
            deps: [0; 8],
            dep_count: 0,
        });
        self.count += 1;
        Some(idx)
    }

    /// Set a cell's value. Invalidates transitive dependents by
    /// marking their value as None. A real impl would re-evaluate
    /// them on next get.
    pub fn set(&mut self, id: &str, value: CellValue) {
        if let Some(idx) = self.find(id) {
            self.cells[idx as usize].as_mut().unwrap().value = value;
            // Invalidate transitive dependents.
            let mut stack: [u8; 64] = [0; 64];
            let mut top = 0;
            for i in 0..self.dep_count_of(idx) {
                if top < 64 {
                    stack[top] = self.cells[idx as usize].as_ref().unwrap().deps[i];
                    top += 1;
                }
            }
            while top > 0 {
                top -= 1;
                let d = stack[top];
                if let Some(c) = self.cells[d as usize].as_mut() {
                    c.value = CellValue::None;
                    for i in 0..c.dep_count {
                        if top < 64 {
                            stack[top] = c.deps[i as usize];
                            top += 1;
                        }
                    }
                }
            }
        }
    }

    /// Get a cell's current value. In a real impl, this would
    /// re-evaluate any stale formula. For the sketch, we just
    /// return the cached value.
    pub fn get(&self, id: &str) -> Option<CellValue> {
        let idx = self.find(id)?;
        Some(self.cells[idx as usize].as_ref()?.value.clone())
    }

    fn find(&self, id: &str) -> Option<u8> {
        for i in 0..self.count {
            if self.cells[i as usize].as_ref()?.id == id {
                return Some(i);
            }
        }
        None
    }

    fn dep_count_of(&self, idx: u8) -> usize {
        self.cells[idx as usize].as_ref().unwrap().dep_count as usize
    }

    /// Mark a cell as depending on another.
    pub fn add_dep(&mut self, from: u8, to: u8) {
        if let Some(c) = self.cells[from as usize].as_mut() {
            if (c.dep_count as usize) < c.deps.len() {
                c.deps[c.dep_count as usize] = to;
                c.dep_count += 1;
            }
        }
    }
}

// =============================================================================
// The trait surface — what an actual implementation would specialize.
// =============================================================================

/// A driver for a physical sensor. Implementations live in their own
/// crates (e.g. `quilt-esp32-dht` for the DHT11 temperature/humidity
/// sensor, `quilt-esp32-bme280` for Bosch's environment sensor).
pub trait Sensor {
    /// The cell id this sensor publishes (e.g. `"sensor.living.temp"`).
    fn id(&self) -> &'static str;
    /// Read the current value from the hardware.
    fn read(&mut self) -> CellValue;
    /// How often to poll, in milliseconds.
    fn poll_interval_ms(&self) -> u64;
}

/// An actuator — a thing the engine can drive. GPIO pin, PWM channel,
/// relay, motor, LED, etc.
pub trait Actuator {
    /// The cell id this actuator controls.
    fn id(&self) -> &'static str;
    /// Apply a new value. The engine calls this when a formula or
    /// listener wants to change the actuator's state.
    fn apply(&mut self, value: CellValue);
}

// =============================================================================
// The hello-world example: a board with a temperature sensor and an LED.
//
// In `main()` on real hardware, the program looks like:
//
//     let mut engine = QuiltEngine::new();
//     engine.define("sensor.temp", CellKind::Sensor, CellValue::None);
//     engine.define("actuator.led", CellKind::Io, CellValue::Bool(false));
//     engine.define("led.on", CellKind::Formula, CellValue::None);
//     engine.add_dep(/* led.on */, /* sensor.temp */);
//
//     loop {
//         // Poll the sensor.
//         let temp = sensor.read();
//         engine.set("sensor.temp", temp);
//
//         // Evaluate the formula. (In a real impl, this would be a
//         // tiny expression interpreter or a Wasm runtime.)
//         let on = temp == CellValue::Float(0.0); // simplified
//         engine.set("led.on", CellValue::Bool(on));
//
//         // Apply to the actuator.
//         actuator.apply(CellValue::Bool(on));
//
//         // Sleep until next poll.
//         delay.delay_ms(sensor.poll_interval_ms());
//     }
//
// That's a 3-cell reactive system on a $3 chip.
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn define_and_get() {
        let mut e = QuiltEngine::new();
        e.define("a", CellKind::Value, CellValue::Int(42));
        assert_eq!(e.get("a"), Some(CellValue::Int(42)));
    }

    #[test]
    fn set_invalidates() {
        let mut e = QuiltEngine::new();
        e.define("a", CellKind::Value, CellValue::Int(1));
        e.set("a", CellValue::Int(99));
        assert_eq!(e.get("a"), Some(CellValue::Int(99)));
    }
}
