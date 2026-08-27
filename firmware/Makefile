# firmware/Makefile — host harness + ESP32 firmware build.
#
# The firmware/ root copies (qm_serve.{c,h}, qm_opcodes.{c,h}, qm_tables.h,
# vm/quilt_vm.{c,h}, critic_gate.{c,h}) are the single editable source of
# truth. src/ holds copies for the pio build (pio compiles src_dir) — run
# `make sync` after editing the root copies so src/ matches; `make gen`
# regenerates src/qm_prog.h; `make reflex-gen` regenerates
# src/reflex/gate_qm.h from the minted critic-gate.qm.

.DEFAULT_GOAL := run
.PHONY: gen host run clean sync fw fw-clean reflex-gen reflex-host reflex-run reflex-fw reflex-clean opcodes-host opcodes-run opcodes-clean merge-s3 nmea-gen nmea-host nmea-run nmea-fw nmea-clean merge-nmea-s3

gen:
	mkdir -p src/vm
	python3 qm2c.py blink.qm signals.json 0 src/qm_prog.h

host: gen
	gcc -O2 -std=c99 -I. -Ivm -Isrc host/main.c qm_serve.c qm_opcodes.c vm/quilt_vm.c -o host_blink

run: host
	./host_blink

sync:
	mkdir -p src/vm src/reflex
	cp qm_serve.c qm_serve.h qm_opcodes.c qm_opcodes.h qm_tables.h src/
	cp vm/quilt_vm.c vm/quilt_vm.h src/vm/
	cp critic_gate.c critic_gate.h src/reflex/

fw: gen sync
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR)

fw-clean:
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR) -t clean

# ── opcodes lane: the five canon opcodes (Paper 211) on the vendored VM ──

opcodes-host: gen
	gcc -O2 -std=c99 -I. -Ivm -Isrc host_opcodes/main.c qm_opcodes.c qm_serve.c vm/quilt_vm.c -o host_opcodes_bin

opcodes-run: opcodes-host
	./host_opcodes_bin

opcodes-clean:
	rm -f host_opcodes_bin

# merged esp32s3 image (same layout as reflex-arc: S3 bootloader at 0x0,
# no boot_app0; bundled esptool v4.11 spells the subcommand merge_bin)
merge-s3: fw
	python3 $(HOME)/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 \
	    merge_bin -o dist/opcodes-s3-merged-0x0.bin \
	    0x0 .pio/build/esp32s3/bootloader.bin \
	    0x8000 .pio/build/esp32s3/partitions.bin \
	    0x10000 .pio/build/esp32s3/firmware.bin

# ── nmea lane: the vessel limb — NMEA 0183 in, sheet cells + rules out ──
# vessel-demo.qm is authored here (no upstream mint): --mint seals the
# receipt (sha256 of the file with the receipt zeroed) into the .qm, then
# compile validates it into src/nmea/vessel_qm.h.

nmea-gen:
	mkdir -p src/nmea
	python3 qm_vessel2c.py vessel-demo.qm src/nmea/vessel_qm.h

nmea-host: nmea-gen
	gcc -O2 -std=c99 -Wall -I. -Ivm -Isrc/nmea -Isrc host_nmea/main.c nmea.c nmea_limb.c qm_opcodes.c vm/quilt_vm.c -o host_nmea_bin

nmea-run: nmea-host
	./host_nmea_bin vessel-stream.txt

nmea-fw: nmea-gen sync-nmea
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR) -e nmea

nmea-clean:
	rm -f host_nmea_bin src/nmea/vessel_qm.h

# sync the nmea lane into the pio tree (nmea env compiles src/nmea/ + the
# shared canon/vm copies already in src/ — see platformio.ini nmea_limb)
sync-nmea:
	mkdir -p src/nmea
	cp nmea.c nmea.h nmea_limb.c nmea_limb.h src/nmea/

# merged esp32s3 image (same layout as the opcodes/reflex lanes)
merge-nmea-s3: nmea-fw
	python3 $(HOME)/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 \
	    merge_bin -o dist/vessel-nmea-s3-merged-0x0.bin \
	    0x0 .pio/build/nmea/bootloader.bin \
	    0x8000 .pio/build/nmea/partitions.bin \
	    0x10000 .pio/build/nmea/firmware.bin

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
