# Makefile — host-side build for the Quilt VM unit tests.
#
# These tests do not need an ESP32. They exercise the
# substrate directly. Build with `make` and run `./test_vm`.
#
# The 5 opcodes are the runtime. C is the desert. The ESP32
# is the open range.

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -Iinclude

all: test_vm

test_vm: tests/test_vm.c src/quilt_vm.c include/quilt_vm.h
	$(CC) $(CFLAGS) -o test_vm tests/test_vm.c src/quilt_vm.c

test: test_vm
	./test_vm

clean:
	rm -f test_vm
