# reflex-arc — the critic's frozen gate on ESP32-S3

The cell-cascade critic's cost-0 6-channel band gate, exported integer-only
(`critic-gate.qm`, micro-units 1e-6) and replayed against the desktop gate
on real critique vectors over UART. Radio dark. Full account:
`../docs/REFLEX-ARC-2026-08-26.md`.

## Layout

| file | role |
|---|---|
| `critic_gate.{c,h}` | the integer judge — compiled UNCHANGED into firmware and host harness |
| `qm_gate2c.py` | compiles `critic-gate.qm` → `src/reflex/gate_qm.h` (bands table + sha receipt) |
| `src/reflex/reflex_main.cpp` | firmware: UART replay protocol, boot receipt, WiFi off + BT stopped |
| `host_reflex/main.c` | host replay: same judge, corpus vs desktop reference, histogram, findings |
| `../tools/reflex/` | corpus builder (plainsong venv), UART replay, vectors/reference data |

## Build

```sh
make reflex-host && cd ../tools/reflex && ../../firmware/host_reflex_bin \
    vectors.txt ref.txt vectors-anchors.jsonl findings.json
make reflex-fw          # pio env reflex_arc (esp32-s3-devkitc-1)
```

## Flash (Casey's line — merged image, one shot)

```sh
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    write_flash -z 0x0 dist/reflex-arc-merged-0x0.bin
```

(Windows/py style, like the limb-blink spike:
`py -m esptool --chip esp32s3 --port COM5 --baud 921600 write_flash -z 0x0 dist/reflex-arc-merged-0x0.bin`)

**Success looks like** — open the monitor (115200) and press EN/reset:

```
reflex-arc v1 — the critic's frozen gate on ESP32-S3 — no cloud, no model, no radio
gate: critic-gate.qm from gate/gate-bands.json v3
mint-receipt sha256: 9c896bb021e87b782c6a00121af830db9f86d9ded49ba6b3e6c35c4213a7e3e4
fixed-point: micro-units (1 unit = 1e-6), integer-only — no floats on target
scope: 6-channel band gate + gray zone; voice-leading/tension-curve/arc stay desktop
channels (µ): note_density[150000,600000] syncopation[200000,1000000] register_spread[50000,250000] rest_ratio[0,300000] harmonic_tension[150000,763000] interval_size[0,650000]
ready — V <id> <f1..f6 µ> · P <id> <chidx> <µ> · B
radio: WiFi off, BT stopped — dark
```

Then run the replay (pyserial):

```sh
python3 tools/reflex/replay_uart.py --port /dev/ttyACM0 \
    --corpus tools/reflex/vectors.txt --ref tools/reflex/ref.txt \
    --anchors tools/reflex/vectors-anchors.jsonl \
    --qm firmware/critic-gate.qm --out-dir tools/reflex/board
```

It verifies the sha receipt, feeds all 80 vectors + 20 anchors, and prints
agreement + the board's own latency histogram; findings land in
`tools/reflex/board/findings-board.json`.

## Manual smoke

In any serial terminal (115200): send

```
V 1 340000 500000 120000 50000 600000 400000
```

expect `R 1 0 0 0 0 0 0 0 0 0 0 0` (all ok, accept — 0 µs compute is
normal: the judge is ~50 ns) and no D lines. A gray-zone poke:

```
V 2 340000 190000 120000 50000 600000 400000
```

→ syncopation 0.19 vs [0.2, 1.0] is gray: sev line shows `1` in slot 2,
`R` carries penalty 400000, verdict stays 0 (accept, single warn), and a
`D` dissent line follows (near edge + gray).
