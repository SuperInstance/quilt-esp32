# firmware/Makefile — host harness + ESP32 firmware build.
#
# The firmware/ root copies (qm_serve.{c,h}, qm_tables.h, vm/quilt_vm.{c,h},
# critic_gate.{c,h}) are the single editable source of truth. src/ holds
# copies for the pio build (pio compiles src_dir) — run `make sync` after
# editing the root copies so src/ matches; `make gen` regenerates
# src/qm_prog.h; `make reflex-gen` regenerates src/reflex/gate_qm.h from
# the minted critic-gate.qm.

.DEFAULT_GOAL := run
.PHONY: gen host run clean sync fw fw-clean reflex-gen reflex-host reflex-run reflex-fw reflex-clean

gen:
	mkdir -p src/vm
	python3 qm2c.py blink.qm signals.json 0 src/qm_prog.h

host: gen
	gcc -O2 -std=c99 -I. -Ivm -Isrc host/main.c qm_serve.c vm/quilt_vm.c -o host_blink

run: host
	./host_blink

sync:
	mkdir -p src/vm
	cp qm_serve.c qm_serve.h qm_tables.h critic_gate.c critic_gate.h src/
	cp vm/quilt_vm.c vm/quilt_vm.h src/vm/

fw: gen sync
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR)

fw-clean:
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR) -t clean

# ── reflex-arc: the critic's frozen gate on metal ─────────────────────────
# critic-gate.qm is minted on the cell-cascade side:
#   cd ../cell-cascade && npm run export:gateqm -- --out ../quilt-esp32/firmware/critic-gate.qm

reflex-gen:
	mkdir -p src/reflex
	python3 qm_gate2c.py critic-gate.qm src/reflex/gate_qm.h

reflex-host: reflex-gen sync
	gcc -O2 -std=c99 -I. -Isrc -Isrc/reflex host_reflex/main.c critic_gate.c -o host_reflex_bin

reflex-run: reflex-host
	./host_reflex_bin

reflex-fw: reflex-gen sync
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR) -e reflex_arc

reflex-clean:
	rm -f host_reflex_bin src/reflex/gate_qm.h

clean:
	rm -f host_blink src/qm_prog.h
