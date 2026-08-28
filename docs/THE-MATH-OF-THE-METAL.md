# The Math of the Metal
### The quilt-on-ESP32 stack, from the assembly up — a three-model round table, verified against the code

*2026-08-27. Iterative cross-model discussion (Claude Haiku 4-5, KimiCode K3, DeepSeek-chat), then verified against the actual firmware. Rspurlock's `framework-arduinoespressif32` = repackaged Arduino-ESP32 core v4.4 (7 commits, default config, no custom patches) — checked on disk.*

## The verdict on the framework question

**The repackaged framework is mathematically inert** — all three models converged independently. It changes packaging, not equations: prescaler values, `esp_timer`'s 1 µs epoch, and GPIO register maps are inherited bit-for-bit from upstream v4.4. What it *does* change is **provenance**: a frozen toolchain (GCC for Xtensa, `-Os`, no LTO) and a pinned core = a reproducible ε. It pins *constants*, and constants are what the math is made of.

## Layer 1 — NMEA in 32-bit µ-units, no FPU (verified against `src/nmea/nmea.c`)

The code already does the correct thing, independently of the discussion:

- `nmea_fixed_to_u`: `u = ip*1000000LL + fr*1000000LL/scale` — **64-bit intermediate**, exactly the overflow-safe path the models derived (the naive 32-bit `centminutes × 1e6` would overflow at 60 billion).
- Bounds safe: |lat| ≤ 90° → 90e6 µ° < 2^31 ✅; |lon| ≤ 180° → 180e6 µ° < 2^31 ✅.
- Depth/wind use **fixed-point multipliers** (`×3048/10000`, `×5/18`) — integer arithmetic only, 19 occurrences of 64-bit in the parser. No FPU anywhere.
- **Toolchain truth (DeepSeek's correction):** under `-Os`, the `/1852`-class divides call `__divsi3` (~30 cycles) and the 64-bit intermediates call `__muldi3` (~100 cycles) — not the idealized 1-cycle `mull`. Real cost per coordinate parse: **~150 cycles ≈ 625 ns @240 MHz**, ~4.7× the naive estimate. Still free against a 1 Hz update.

## Layer 2 — Keel timing ♩=60 (Kimi's math)

- S3 timers run off APB = 80 MHz (240 MHz PLL / 3). Exact 1 s needs N·d = 80,000,000 — satisfied exactly by d=80, N=1,000,000 (`esp_timer` path): **0 ppm software error**.
- Residual = 40 MHz crystal ±20 ppm → **72 ms/hr worst case** (~1.7 s/day).
- `digitalWrite` = one `s32i.n` into GPIO_OUT_W1TS/W1TC (0x60004008/0C); toggle ≈ 3 ops ≈ 17 ns. No FPU, no 64-bit ALU on LX7 — u64 compares compile to two 32-bit sub/bne pairs.

## Layer 3 — The tick budget (Kimi's dispatch math)

Dense opcodes 0..5, `-Os` compare-chain ≈ 7 cycles/cell; a 100-cell tick with 200 edges, worst case all EFFECT:

    T = 100×(1+7) + 200×7 = 2,200 cycles ≈ 9.2 µs @ 240 MHz

That's 1/108,000 of the keel period. **The opcode loop is free**; the FreeRTOS tick quantum (1-10 ms) that schedules it is 100-1000× larger than the work. The one asymptotic knob: **locality** — placing the cell graph in PSRAM turns a 2,200-cycle tick into ~30,000+ (cold-line penalties). Internal SRAM = instruction-count-bound; PSRAM = memory-bound.

## The dispute, resolved honestly

- **Claude claimed a ~100 ms sampling skew** between NMEA arrival timestamps and keel GPIO observations (UART FIFO drain vs ISR phase).
- **Kimi countered:** both run off the shared S3 SYSTIMER epoch (16 MHz → 62.5 ns quantum); aliasing exists only below 62.5 ns — six orders of magnitude under anything the keel does. The 100 ms figure is *data age*, not clock skew.
- **Resolution:** Kimi is right about the clocks; Claude's point survives as *data freshness* — a heading derived from two positions timestamped at packet-arrival is biased by UART latency if the vessel is turning (1-2° course error). Fix if it ever matters: timestamp inside `uart_event_t` callback, not after sentence assembly. Filed as a watch item, not a defect.

## The one real lever

Nothing in the repackaged framework changes any number above. What changes the math is one decision the codebase owns: **where the cell graph lives** (SRAM vs PSRAM). The framework question is settled; the locality question is open.

*Round-table transcript: `~/.openclaw/workspace/scratch/fw-math/` (r1-*, r2-*). Verdict: adopt rspurlock's package as a pinned provenance if reproducibility matters more than upstream freshness — the math won't notice either way.*
