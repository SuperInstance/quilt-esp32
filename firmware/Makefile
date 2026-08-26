# firmware/Makefile — host harness + ESP32 firmware build.
#
# The firmware/ root copies (qm_serve.{c,h}, qm_tables.h, vm/quilt_vm.{c,h})
# are the single editable source of truth. src/ holds copies for the pio
# build (pio compiles src_dir) — run `make sync` after editing the root
# copies so src/ matches; `make gen` regenerates src/qm_prog.h.

.DEFAULT_GOAL := run
.PHONY: gen host run clean sync fw fw-clean

gen:
	mkdir -p src/vm
	python3 qm2c.py blink.qm signals.json 0 src/qm_prog.h

host: gen
	gcc -O2 -std=c99 -I. -Ivm -Isrc host/main.c qm_serve.c vm/quilt_vm.c -o host_blink

run: host
	./host_blink

sync:
	mkdir -p src/vm
	cp qm_serve.c qm_serve.h qm_tables.h src/
	cp vm/quilt_vm.c vm/quilt_vm.h src/vm/

fw: gen sync
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR)

fw-clean:
	$(HOME)/.platformio/penv/bin/pio run -d $(CURDIR) -t clean

clean:
	rm -f host_blink src/qm_prog.h
