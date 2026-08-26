# VESSEL-FIT — quilt-esp32 against F/V EILEEN

*Playtest lane, 2026-08-26. The vessel: a fishing boat in Kodiak, AK. The brain runs offline 60 mi offshore. Doctrine: ESP32 + Liquid LFM2.5, "hundred boats", zero per-token cost. What I actually ran on this machine today, and what the metal lane should do next.*

---

## 0. What I ran today

| Check | Result |
|---|---|
| `make run` (host blink harness, real quilt_vm.c + qm_serve.c) | ✅ compiles clean, `qm2c.py blink.qm → qm_prog.h` (6 binds, 1 link, 2 rules, 3 views, 5 signals), ticks produce `on/off/table-miss` LED JSON, `{"ok":true}`. |
| `make reflex-host` | ✅ builds `host_reflex_bin`; `qm_gate2c.py` regenerates gate from `critic-gate.qm` with the same mint-receipt sha (`9c896bb0…`). |
| `tools/reflex` replay: `host_reflex_bin vectors.txt ref.txt vectors-anchors.jsonl findings.json` | ✅ **100.0000% agreement** — 480/480 channel readings, 80/80 bar verdicts, 20/20 anchor probes, zero divergences. Desktop latency accept p50=40ns / p99=101ns. Dissent ledger written. |
| `dist/` | merged flash images present for limb-blink (esp32dev + esp32s3) and reflex-arc, with SHA256SUMS. |

**Verdict:** this repo is the boat's nerve endings. Rule tables compile to C, run on a $3 chip radio-dark, at 40ns-class latencies, with a hash-sealed receipt at boot. That is exactly what a fishing vessel wants: things that fail loudly and locally, never phoning home.

---

## 1. The sensor-limb pattern (generalizes from what's proven)

What blink + critic-gate jointly prove, as a recipe:

1. **Encode domain knowledge as a `.qm` rule table** — bands over inputs, actions per band. blink = LED phase bands; critic-gate = 6-channel quality bands (micro-units, integer-only, no floats).
2. **Compile ahead-of-time** (`qm2c.py` / `qm_gate2c.py`) to a C table + sha256 receipt. The chip never parses; it serves lookups.
3. **Radio dark by default** — firmware boots with WiFi off, BT stopped (`src/reflex/reflex_main.cpp`), receipt printed over UART.
4. **UART as the only hole** — `V`/`P`/`B` replay protocol. One wire in, one wire out, auditable.
5. **Dissent ledger** — disagreements between metal and reference get logged (JSONL), which is escalation wiring in embryo: the limb doesn't argue, it *records* the argument for the next tier up.

For the boat, a **sensor limb** is: physical sensor → ADC/UART → `.qm` band table → local LED/buzzer + escalation line. Zero cloud, zero model, survives salt air and a dead helm computer.

---

## 2. Boat network sketch (UART + ESP-NOW)

```
 [engine sensors]      [AIS rx]           [cameras]
  ESP32-S3 limb         ESP32-S3 limb      ESP32-S3 limb
  rpm/temp/pressure     .qm distance       motion/log score
  .qm band table        bands              threshold
        |                    |                  |
        |     ESP-NOW (no router, no AP, ~200m LOS deck-scale)
        v                    v                  v
        +---------+----------+---------+--------+
                  | UART ( wired, helm )
                  v
        helm box (RPi-class): quilt-rust sheet
        eileen-helm.yaml  — see quilt-rust/docs/VESSEL-FIT.md
                  |
        local Liquid LFM2.5 critique (critic-gate pattern,
        already 100% replay agreement on this metal lane)
```

- **ESP-NOW** between limbs: connectionless, no AP/router to fail, works with WiFi *off* — matches the radio-dark doctrine deck-scale.
- **Wired UART** limb→helm for anything safety-relevant (engine): wireless is a convenience, not a dependency.
- Each limb keeps its own mini journal (the dissent-ledger format is already proven) — helm down, the limb still blinks and buzzes and logs.

---

## 3. What today's firmware proves *for the boat*

- **Deterministic safety reactions don't need a model, and shouldn't have one.** RPM redline, oil pressure low, AIS contact closing — all are band tables, served in ~40–100ns, on hardware that costs less than the fuel to motor past them.
- **The critic-gate mint-receipt pattern is the boat's trust anchor.** Boot prints sha256 of the rule table it's running. After a season, you can prove what thresholds were live on any given day — that's a lot more than most marine electronics can say.
- **Integer-only micro-units work.** No FPU dependence, no float edge cases across toolchains — 480/480 replay agreement is the evidence.

## 4. Next metalstone: **AIS-in → rule table → alert LED**

The single highest-value next flash. Concretely:

- Input: NMEA-0183 AIVDM from the helm AIS transponder (serial 38400) into an ESP32-S3 UART.
- Parse just enough: own position/COG/SOG (from GPS sentences) + nearest contact position/SOG/COG → relative distance and closing rate (haversine + relative-velocity in fixed-point micro-units — the critic-gate's integer discipline).
- `.qm` rule table: distance bands (e.g. >6nm clear / 6–2nm watch / 2–1nm caution / <1nm alert) crossed with closing-rate bands → LED color + buzzer pattern.
- Escalation line to helm box via UART, dissent-log format for misses.
- Radio dark, mint-receipt at boot, host-replay harness like reflex-arc.

Why this one first: it exercises *every* proven primitive (serial in, integer math, band table, escalation, replay verification) on a use-case with real safety payoff, and it's the first limb whose input is a live external feed rather than a generated corpus.

---

*Evidence: `make run` transcript, reflex replay output (480/480, 80/80, 20/20), and the existing `docs/REFLEX-ARC-2026-08-26.md` / `docs/MILESTONE-2026-08-26.md` accounts. All reproduced on this machine 2026-08-26.*
