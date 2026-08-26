# Pass B brief — ESP32 firmware (blink-from-table) + spike report  (ARCHIVED 2026-08-26)

> Archived copy of the opencode Pass B spec. Kept as the spec-of-record.

Extend the Pass A work in `/home/eileen/projects/quilt-esp32/firmware/` to the
on-device milestone: an ESP32 DevKit V1 blinks its GPIO2 LED **driven by the
.qm rule table through the real quilt-vm-c**. No cloud, no model, no WiFi.

## New/changed files

1. **Makefile** — `gen` emits `src/qm_prog.h`; host builds with `-Isrc`; add `fw`, `fw-clean`, `sync`.
2. **`platformio.ini`** — `[env:esp32dev]` espressif32/arduino, monitor 115200, upload 921600, `-DCORE_DEBUG_LEVEL=0`; `[platformio] src_dir = src`.
3. **`src/`** — copies of qm_serve/qm_tables/vm + generated qm_prog.h + main.cpp (Makefile `sync` refreshes).
4. **`src/main.cpp`** — Arduino firmware: banner, facts line; loop = 500 ms phase on/off → QmSignal{to:"led-limb",kind:"tick",payload phase canon} → qm_serve → qm_led_from_response → digitalWrite(GPIO2); miss holds LED; JSON log per serve; `[hb]` heartbeat every 40 serves. NO WiFi/BLE.
5. **`README-SPIKE.md`** — milestone, file map, host evidence, reference-VM equivalence, build section, `<!-- FLASH-COMMANDS -->` + `<!-- BLOCKERS -->` placeholders, "What is NOT proven yet".
6. Prove `make -C firmware run` green AND `pio run` (xtensa) green; include both outputs verbatim; report portability fixes (never semantics).

## Rules
- firmware/ only, no new deps, no lib_deps, Arduino framework only.
