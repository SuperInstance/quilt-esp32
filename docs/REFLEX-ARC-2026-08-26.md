# REFLEX-ARC — the critic's frozen gate on metal, judged identical (2026-08-26)

> Branch `reflex-arc` (this repo) · export side: cell-cascade branch
> `reflex-arc` (`docs/reflex-arc-export.md`). Failures are first-class
> below — read them before the numbers.

## What this is

The cortex critic's **cost-0 six-channel band gate** — `note_density,
syncopation, register_spread, rest_ratio, harmonic_tension, interval_size`
× intent bands + gray zone, the frozen tissue of cell-cascade
`src/critic.ts` — exported integer-only onto the ESP32-S3 and replayed
against the desktop gate on **real critique vectors** harvested from the
organism's runs. Radio dark. No WiFi, no model, no Mint-Protocol
generalization (kimi's rule: no abstraction until a second artifact forces
the pipe — this is artifact #1).

**Result (host metal-code replay, 2026-08-26):**

| measure | count | agree |
|---|---|---|
| channel readings (80 real bars × 6) | 480 | **480 — 100.0000%** |
| bar verdicts (penalty + accept/revise + gray flags) | 80 | **80 — 100.0000%** |
| ledger anchor probes (the tick-log's own logged readings) | 20 | **20 — 100.0000%** |
| divergences (pre-registered: table inexpressiveness, band-edge rounding) | — | **zero** |

Judge latency, same C binary as the firmware, desktop -O2
(committed evidence: `tools/reflex/findings.json`):
accept p50 **20 ns** / p99 70 ns · revise p50 **20 ns** / p99 40 ns —
the gate is faster than the board's µs timer resolution; the board stamps
0–1 µs and that is the honest reading, not a rounding trick.

## The chain (mint to metal)

```
cell-cascade gate/gate-bands.json (v3, the mint's artifact)
        │  npm run export:gateqm        ← sha256 receipt baked in
        ▼
critic-gate.qm  (blink-style .qm, integer micro-units, data-only)
        │  qm_gate2c.py                 ← validated at compile time
        ▼
gate_qm.h  (C tables: bands, ambiguity, penalties, dissent ε, sha)
        │
        ▼
critic_gate.c  — the integer judge, compiled UNCHANGED into:
        ├── firmware/src/reflex/reflex_main.cpp  (ESP32-S3, UART replay, radio dark)
        └── firmware/host_reflex/main.c          (desktop replay = the proof above)
```

The 5-opcode quilt VM stays out of the numeric path on purpose: its one
frozen expression (`sigma_distance`) is float, and the format-freeze
doctrine plus kimi's no-generalization rule both say — don't extend the VM
for artifact #1. The `.qm` is the frozen **data** artifact the mint owns;
the judge is dedicated fixed-point C. When a second artifact forces a
pipe, that's the seam to revisit.

## Fixed point: micro-units (1 µ = 10⁻⁶), not Q16.16

Documented choice (cell-cascade `docs/reflex-arc-export.md` has the full
argument): every number in the gate's pipeline is decimal by construction
— ear features rounded to 6dp, bands at 3dp (the mint moved an edge to
**0.763**), gray zone 0.06, penalties 0.4/1.0. All are **exact** on the
10⁻⁶ grid as signed 32-bit integers (max 1.6 = 1,600,000 µ). Q16.16
represents none of 0.06/0.763/0.4 exactly (undyadic) — ±2.4·10⁻⁶ error at
band edges, exactly where comparisons live. Micro-units are bit-exact
against the desktop's decimal floats by construction; the 500-vector
replay proved it empirically. Float→µ conversion happens exactly once, at
export, behind a round-trip guard that refuses lossy values.

Semantics mirror `cheapCritique`'s band branch exactly — bad past the gray
zone, warn (gray) inside it, **inclusive edges** (v == lo/hi is ok),
penalties 400,000/1,000,000 µ, revise at ≥ 1,000,000 µ.

## Provenance on metal — the mint receipt

`critic-gate.qm` carries the sha256 of the exact `gate-bands.json` bytes
it was minted from; `gate_qm.h` embeds it; the firmware prints it at boot
and the UART replay **refuses to judge** if the board's receipt doesn't
match the corpus's. Current receipt:

```
9c896bb021e87b782c6a00121af830db9f86d9ded49ba6b3e6c35c4213a7e3e4
```

## The corpus (real vectors, honest accounting)

Built by `tools/reflex/build_corpus.py` (run with the plainsong venv — the
same deterministic ear the runs used) from cell-cascade `runs/`:

- 66 distinct `@piano` bars from every run's `.song` (final accepted bars)
- candidate bars from every tick-log `answer_head` (compose + escalated
  arrange), voice-normalized exactly as the driver did
- 7 arranger stock bars (reconstructed on the cell-cascade side through
  the real `stockBarFor`, core-drift guarded)
- re-measured per bar by plainsong `analyze_features` (voice=piano);
  per-bar features are bar-local, so context-free re-measurement is exact
- **80 bars = 480 readings**, plus the **20 logged `gate`-evidence
  readings** as single-channel anchor probes
- **all 20 anchors matched the reconstruction exactly** — the corpus is
  validated against the ledger, not just claimed

### Finding F1 (filed): the ledger cannot feed a 500-vector replay by itself

The tick-log records only violation/gray **evidence** (20 readings across
19 runs) — not every vector the gate judged. The ≥500 bar was met at
**exactly 500** (480 + 20) by reconstruction from logged artifacts;
padding was available (bass/drums bars the gate never judged) and refused.
Fix if this lane matters: the driver logs the full six-number trace per
critique. Also: evidence values are 3dp-rounded (`r3`) while the gate
judges 6dp — anchors compare at logged precision.

### Findings F2–F3 (filed, pre-registered classes): none observed

Zero divergences of either pre-registered class — no table
inexpressiveness (the band gate is pure comparisons; nothing the desktop
gate expresses fails to fit the integer table) and no band-edge rounding
(the µ grid makes edges exact). The absence is a result, not an omission:
`tools/reflex/findings.json` carries the empty divergence list with the
pre-registration labels.

## Radio dark

`setup()`: `WiFi.mode(WIFI_OFF)` + `btStop()`, then the banner says so.
No radio API is ever initialized after; the replay is the only I/O.

## Dissent logging (the stretch seed — Claude's seam, adopted)

The judge flags any reading within **ε = 20,000 µ (0.02)** of a band edge
or in the gray zone; the firmware streams `D <vec> <ch> <value> <edge>
<dist>` lines and both harnesses write a dissent ledger
(`dissent-ledger-host.jsonl`, `board/dissent-ledger-board.jsonl`). On the
corpus: **85 dissent flags** over 480 readings — the near-edge tissue the
escalation seam would adjudicate. This is the embryo wiring only: nothing
escalates yet (the `.qm`'s `escalations` is empty, honestly).

## Latency histogram (per verdict)

Desktop, same C as metal (`-O2`, ns via `clock_gettime`):

| verdict | n | p50 | p99 |
|---|---|---|---|
| accept | 73 | 20 ns | 70 ns |
| revise | 7 | 20 ns | 40 ns |

The board stamps `esp_timer` µs — expect 0–1 µs per judgment (the gate
outruns the timer; report the stamps as measured, don't upscale).
Board-side numbers land in `tools/reflex/board/findings-board.json` when
Casey runs the ceremony below.

## The board ceremony (Casey flashes)

1. Build: `cd firmware && make reflex-fw` (already built here;
   merged image at `firmware/dist/reflex-arc-merged-0x0.bin`,
   sha256 `e8d789c376995e9992a9b34bc8989b621692c143b2dd18f927650f8d7982da2a`).

2. Flash (Linux):

   ```sh
   python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
       --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
       write_flash -z 0x0 firmware/dist/reflex-arc-merged-0x0.bin
   ```

   Windows: `py -m esptool --chip esp32s3 --port COM5 --baud 921600 write_flash -z 0x0 firmware\dist\reflex-arc-merged-0x0.bin`

3. Success looks like: monitor at 115200 shows the banner with
   `mint-receipt sha256: 9c896bb0…`, `radio: WiFi off, BT stopped — dark`,
   then `ready — V <id> <f1..f6 µ> · P <id> <chidx> <µ> · B`.

4. Replay:

   ```sh
   python3 tools/reflex/replay_uart.py --port /dev/ttyACM0 \
       --corpus tools/reflex/vectors.txt --ref tools/reflex/ref.txt \
       --anchors tools/reflex/vectors-anchors.jsonl \
       --qm firmware/critic-gate.qm --out-dir tools/reflex/board
   ```

   Expected: `100.0000%` on all three counters, `exit 0`. Anything else:
   the findings file IS the result — file it.

## Failures first (the healing log)

- **3/20 anchors unmatched on the first corpus pass.** Three real causes
  found in order: the driver's 3dp evidence rounding (compare at logged
  precision); Python banker's-rounding disagreeing with JS `Math.round`
  at exactly 0.3125 (checker now rounds half-up like the driver's `r3`);
  one bar (the A7b9 stock serve) existing only in the arranger table
  (reconstructed via cell-cascade `reflex_stock_bars.ts` — 20/20 after).
- **Corpus under 500 on distinct bars.** 77 first, 80 after stock bars —
  480 readings. The bar was met by adding the 20 ledger anchors as
  probes, not by padding. F1 filed.
- **toMicro's first grid guard was dead code** (threshold 0.5 after
  rounding can never trip — rounding always lands within 0.5). Caught by
  its own unit test; replaced with a round-trip guard. Lesson exported to
  the cell-cascade doc: a dead guard is worse than none.
- **Anchor probe parse in the host harness missed every probe** (sscanf
  pattern didn't consume the leading `{`). Caught because the harness
  prints 0 probes — a count of zero when twenty were expected is a
  failure, not a pass. Fixed; 20/20.
- **esptool merge flag is `merge_bin`** (underscore) in the bundled
  v4.11 — the hyphenated form is v5+. The flash line above uses the
  bundled one; both spellings noted here so future-us doesn't "fix" it
  blindly.
- **The reflex env initially wanted to compile `main.cpp`/`qm_serve.c`**
  (blink sources) — `build_src_filter` now includes only `reflex/` +
  `critic_gate.c`; the blink envs are untouched and still build (RAM
  13.1% / flash 19.9% for reflex_arc).

## Reproduce everything

```sh
# corpus (plainsong venv python)
/home/eileen/projects/plainsong-mcp/.venv/bin/python3 tools/reflex/build_corpus.py \
    --runs /home/eileen/projects/cell-cascade/runs --out tools/reflex/vectors.jsonl \
    --stock tools/reflex/stock-bars.json
# desktop oracle (cell-cascade repo)
npx tsx scripts/reflex_reference.ts --corpus ../quilt-esp32/tools/reflex/vectors.jsonl \
    --out ../quilt-esp32/tools/reflex/ref.jsonl
# host metal-code replay — the 100% proof
cd firmware && make reflex-host && cd ../tools/reflex && ../../firmware/host_reflex_bin \
    vectors.txt ref.txt vectors-anchors.jsonl findings.json
# firmware
cd firmware && make reflex-fw
```
