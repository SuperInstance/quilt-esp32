# CMakeLists.txt — Quilt VM on ESP32, via ESP-IDF.
#
# The herd of ESP32s is the runtime. This build file
# glues the 5-opcode substrate (quilt_vm.c) to the ESP-NOW
# mesh layer (esp_now_mesh.c) to the entry point (main.c).
#
# The 5 opcodes are the runtime. C is the desert. The ESP32
# is the open range.

cmake_minimum_required(VERSION 3.16)

# Set the project to use the ESP-IDF build system.
set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(quilt-esp32)

# The main firmware component — what gets flashed to the
# ESP32. Hosts the 5 opcodes, the ESP-NOW mesh, and the
# entry point.
idf_component_register(
    SRCS
        "src/main.c"
        "src/quilt_vm.c"
        "src/esp_now_mesh.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_wifi
        esp_now
        esp_event
        nvs_flash
        esp_netif
        esp_system
        esp_common
        freertos
        log
)

# The unit tests (test_vm.c) are built by the host Makefile
# at the repo root — they do not need ESP-IDF, only the
# substrate. See README.md for `make test`.
#
# To run them under an ESP-IDF unity harness, create a
# `tests/host_test/CMakeLists.txt` that idf_component_register's
# test_vm.c with quilt_vm.c. The skeleton omits this to keep
# the firmware image small.
