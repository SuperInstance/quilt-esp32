# Pass A brief — blink.qm limb + qm_serve + host harness  (ARCHIVED 2026-08-26)

> Archived copy of the opencode Pass A spec that produced this firmware.
> Kept as the spec-of-record for the pass.

You are writing the first real milestone of quilt-esp32: a **.qm rule-table limb
that blinks an LED**. No cloud, no model, no WiFi. The rule table IS the firmware logic.

## Context (read these first)

- `.qm` format: `/home/eileen/projects/cell-cascade/tools/qm_compiler/examples/cue-tokens.qm` (the exemplar organism — bind/link/effect ops, guards `{kind, payload_equals}`, actions `{set}`, views, routes).
- Compiler (USE AS-IS, vendored): `/home/eileen/projects/cell-cascade/tools/qm_compiler/benchmarks/qm2c.py` — compiles `.qm` + signals fixture → C static tables header. Add a provenance comment when vendoring (source repo + date + branch `edge-benchmarks`).
- Serve semantics (the reference to adapt): `/home/eileen/projects/cell-cascade/tools/qm_compiler/benchmarks/qm_bench.c` — `rule_matches` (kind equality + canonical-JSON `payload_equals` subset by strcmp, first match wins), `serve` (route to `<to>:response`, hit → `qvm_effect` QM_SET + `qvm_tick(1.0)`, miss → `{"miss":true}` in-VM, results report null).
- Real VM: `/home/eileen/projects/quilt-vm-c/src/quilt_vm.{c,h}` — vendor verbatim (add provenance comment at top of the vendored copies, do NOT modify the code).
- qm2c.py emits struct typedefs **in the consuming .c** (see qm_bench.c lines ~25-55: QmKv/QmBind/QmLink/QmRule/QM_SET/QM_EXPR/QmViewDef/QmSignal) — put those typedefs in a `qm_tables.h` that both host and firmware include, since qm2c output assumes they pre-exist.

## Files to create under /home/eileen/projects/quilt-esp32/firmware/

1. **`blink.qm`** — organism `limb-blink`, format qm version 1:
   - bind `limb-root` (totipotent spine stub), `limb-root:facts`, `limb-root:response` = null
   - bind `led-limb` (sclerotic; rule_count:2), `led-limb:facts`, `led-limb:response` = null
   - link `led-limb` --lineage--> `limb-root`
   - effect 1: guard `{"kind":"tick","payload_equals":{"phase":"on"}}` → set `{"led":true}`
   - effect 2: guard `{"kind":"tick","payload_equals":{"phase":"off"}}` → set `{"led":false}`
   - views: `led-limb/facts`, `led-limb/response`, `limb-root/response`
   - routes: `{"tick":"led-limb"}`

2. **`signals.json`** — fixture: on/off alternating, one guard-miss (`phase:"sleep"`) mid-fixture, then on, off.

3. **`qm2c.py`** — vendored copy with provenance header. Do not alter logic.

4. **`vm/quilt_vm.c` + `vm/quilt_vm.h`** — vendored verbatim + provenance header.

5. **`qm_tables.h`** — generated-table struct typedefs + `#include "qm_prog.h"` guarded by QM_PROG_HEADER.

6. **`qm_serve.h` / `qm_serve.c`** — portable serve path (C99): `qm_serve_init`, `qm_serve` (adapt bench serve; drop QM_EXPR branch, return codes, no fprintf/exit), `qm_led_from_response` (1/0/-1), `qm_serve_free`.

7. **`host/main.c`** — harness: one JSON line per signal + final `{"ok":true}`.

8. **`Makefile`** — `gen` / `host` / `run` / `clean`.

9. **Run `make run` and include its output.** Expected: 4 table hits alternating, 1 table-miss. Fix until it passes; do not weaken semantics.

## Rules
- Stay under `firmware/`. No new deps. C99 + stdlib only. Honest reporting.
