//! quilt-esp32 — a sketch of a Quilt runtime for ESP32-class microcontrollers.
//!
//! This is a design sketch, not a working implementation. The shapes here
//! are what we want the API to feel like. When the actual no_std port is
//! written, the cell kinds and reactive DAG will mirror this surface.
//!
//! The point of this file is to answer the question: "what would Quilt
//! look like as an embedded runtime?" — concretely.

#![cfg_attr(target_os = "none", no_std)]

use core::cell::RefCell;
use heapless::{FnvIndexMap, String, Vec};

/// The maximum number of cells a small board can hold. 64 is plenty
/// for a typical sensor/actuator setup; a beefier board can raise this.
pub const MAX_CELLS: usize = 64;
pub const MAX_DEPS: usize = 8;
/// The maximum length of a cell id. 32 bytes is enough for namespaced ids
/// like `sensor.living.temp` or `actuator.kitchen.fan.speed`.
pub const MAX_ID_LEN: usize = 32;

/// The eight cell kinds. Same vocabulary as the larger engines.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CellKind {
    Value,    // a static value
    Formula,  // an expression that depends on other cells
    Program,  // a small expression that runs
    Sensor,    // a named input source
    Api,      // an outbound call (e.g. to a network endpoint)
    Listener, // fires on changes
    Router,   // caller-context-aware dispatch
    Io,       // an outbound port (uart, gpio, i2c, ...)
}

/// The types a cell can hold. We use a tagged union, sized for the
/// constraints of a microcontroller: scalars inline, blobs by reference.
#[derive(Clone, Copy)]
pub enum CellValue {
    None,
    Bool(bool),
    Int(i64),
    Float(f32),
    Str(heapless::String<32>),  // up to 32 chars inline
    Bytes(heapless::Vec<u8, 64>), // up to 64 bytes inline
}

impl CellValue {
    pub fn as_bool(&self) -> Option<bool> { if let Self::Bool(v) = self { Some(*v) } else { None } }
    pub fn as_int(&self) -> Option<i64> { if let Self::Int(v) = self { Some(*v) } else { None } }
    pub fn as_float(&self) -> Option<f32> { if let Self::Float(v) = self { Some(*v) } else { None } }
}

/// A cell, as stored in the engine. We pack the dependencies inline so
/// the whole graph fits in a contiguous block of memory — important
/// for an embedded device with no allocator.
#[derive(Clone)]
pub struct Cell {
    pub id: String<MAX_ID_LEN>,
    pub kind: CellKind,
    pub value: CellValue,
    pub deps: Vec<u16, MAX_DEPS>,  // indices into the engine's cell array
}

/// The engine itself. Holds up to MAX_CELLS cells. Reactive.
/// Designed to live in a single static memory block so it survives
/// reboots and can be snapshotted to flash.
pub struct QuiltEngine {
    cells: Vec<Cell, MAX_CELLS>,
    /// Per-cell version counters. Incremented on every set.
    versions: Vec<u32, MAX_CELLS>,
    /// Time of last evaluation, in milliseconds since boot.
    last_eval_ms: u64,
}

impl QuiltEngine {
    pub const fn new() -> Self {
        Self {
            cells: Vec::new(),
            versions: Vec::new(),
            last_eval_ms: 0,
        }
    }

    /// Define a new cell. Returns the cell's index in the engine.
    /// Panics if MAX_CELLS is exceeded.
    pub fn define(&mut self, id: &str, kind: CellKind, value: CellValue) -> Result<u16, Error> {
        if self.cells.len() >= MAX_CELLS { return Err(Error::OutOfCells); }
        let mut s = String::new();
        s.push_str(id).map_err(|_| Error::IdTooLong)?;
        let mut cell = Cell { id: s, kind, value, deps: Vec::new() };
        // deps are registered in a separate pass
        let idx = self.cells.len() as u16;
        self.cells.push(cell).map_err(|_| Error::OutOfCells)?;
        self.versions.push(0).map_err(|_| Error::OutOfCells)?;
        Ok(idx)
    }

    /// Set a cell's value. Invalidates transitive dependents.
    pub fn set(&mut self, id: &str, value: CellValue) -> Result<(), Error> {
        let idx = self.find(id).ok_or(Error::NoSuchCell)?;
        // Clone the deps out so we don't hold a borrow on self.
        let deps = self.cells[idx as usize].deps.clone();
        // Mark this cell and its dependents stale.
        self.cells[idx as usize].value = value;
        self.versions[idx as usize] = self.versions[idx as usize].wrapping_add(1);
        // Invalidate transitive dependents.
        let mut stack: Vec<u16, MAX_CELLS> = deps;
        while let Some(d) = stack.pop() {
            self.cells[d as usize].value = CellValue::None;
            for dd in self.cells[d as usize].deps.iter() {
                if !stack.contains(dd) {
                    let _ = stack.push(*dd);
                }
            }
        }
        Ok(())
    }

    /// Get a cell's current value. (In a real implementation, this would
    /// re-evaluate any stale formula. For the sketch, we just return the
    /// cached value.)
    pub fn get(&self, id: &str) -> Option<CellValue> {
        let idx = self.find(id)?;
        Some(self.cells[idx as usize].value.clone())
    }

    fn find(&self, id: &str) -> Option<u16> {
        for (i, c) in self.cells.iter().enumerate() {
            if c.id == id { return Some(i as u16); }
        }
        None
    }

    /// Mark a cell as depending on another. Called after a formula
    /// cell is defined.
    pub fn add_dep(&mut self, from: u16, to: u16) -> Result<(), Error> {
        self.cells[from as usize].deps.push(to).map_err(|_| Error::TooManyDeps)
    }
}

#[derive(Debug)]
pub enum Error {
    OutOfCells,
    IdTooLong,
    NoSuchCell,
    TooManyDeps,
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
//     engine.define("sensor.temp", CellKind::Sensor, CellValue::None).unwrap();
//     engine.define("actuator.led", CellKind::Io, CellValue::Bool(false)).unwrap();
//     engine.define("led.on", CellKind::Formula, CellValue::None).unwrap();
//     engine.add_dep(/* led.on */, /* sensor.temp */).unwrap();
//
//     loop {
//         // Poll the sensor.
//         let temp = sensor.read();
//         engine.set("sensor.temp", temp).unwrap();
//
//         // Evaluate the formula. (In a real impl, this would be a tiny
//         // expression interpreter or a Wasm runtime.)
//         let on = temp.as_float().unwrap_or(0.0) > 25.0;
//         engine.set("led.on", CellValue::Bool(on)).unwrap();
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
        e.define("a", CellKind::Value, CellValue::Int(42)).unwrap();
        assert_eq!(e.get("a").unwrap().as_int(), Some(42));
    }

    #[test]
    fn set_invalidates_dependents() {
        let mut e = QuiltEngine::new();
        let a = e.define("a", CellKind::Value, CellValue::Int(1)).unwrap();
        let b = e.define("b", CellKind::Value, CellValue::Int(2)).unwrap();
        e.add_dep(b, a).unwrap();
        // Set 'a' to 99. 'b' should now be stale.
        e.set("a", CellValue::Int(99)).unwrap();
        assert_eq!(e.get("a").unwrap().as_int(), Some(99));
        // In a real impl, b would be re-evaluated. For the sketch, it's
        // marked None.
    }
}
