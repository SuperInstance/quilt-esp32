#!/usr/bin/env bash
# tools/tower-sync.sh -- TWIN-PORT LANE lockstep: re-run the quilt-verilog
# tower compiler over the oil-pressure cell and refresh this repo's copy
# (cells/oil-pressure/), so the two repositories cannot drift silently:
#
#   quilt-verilog/tools/tower/oil-pressure-port.cell.yaml  (L0, the edit set)
#     -> emith.py  -> cells/oil-pressure/tower_oil_pressure_port.c (L2, regenerated)
#     -> verify.py -> the 17-golden-vector gate, must PASS before anything lands
#     -> cp        -> cells/oil-pressure/oil-pressure-port.cell.yaml (L0 copy)
#     -> cp        -> firmware/src/oil_pressure/ (pio tree, same role as
#                     `make -C firmware oil-sync`)
#
# The generated C is a middle layer, never hand-edited here. If this script
# reports a diff you did not expect, the cell changed upstream -- re-run the
# host gate (`make -C firmware oil-run`) and re-audit before committing.
#
#   TOWER_SRC=/path/to/quilt-verilog/tools/tower tools/tower-sync.sh
#   CELL_YAML=other-port.cell.yaml   tools/tower-sync.sh   (future cells)
#
# default TOWER_SRC: <this repo>/../quilt-verilog/tools/tower
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CELL_DIR="$ROOT/cells/oil-pressure"
FW="$ROOT/firmware"
TOWER_SRC="${TOWER_SRC:-$ROOT/../quilt-verilog/tools/tower}"
CELL_YAML="${CELL_YAML:-oil-pressure-port.cell.yaml}"

die() { echo "tower-sync: FAIL: $*" >&2; exit 1; }

[ -f "$TOWER_SRC/emith.py" ]  || die "tower compiler not found at $TOWER_SRC (set TOWER_SRC=...)"
[ -f "$TOWER_SRC/verify.py" ] || die "tower verifier not found at $TOWER_SRC"
[ -d "$CELL_DIR" ]            || die "$CELL_DIR missing -- run from a full checkout"

echo "tower-sync: compiler $TOWER_SRC"
echo "tower-sync: cell     $TOWER_SRC/$CELL_YAML"

# 1. regenerate the L2 C middle layer into this repo
python3 "$TOWER_SRC/emith.py" "$TOWER_SRC/$CELL_YAML" \
    -o "$CELL_DIR/tower_oil_pressure_port.c"

# 2. the upstream Law-5 gate: 17 golden vectors, must PASS
python3 "$TOWER_SRC/verify.py" "$TOWER_SRC/$CELL_YAML"

# 3. refresh the L0 cell copy (the edit set travels with the generated C)
cp -f "$TOWER_SRC/$CELL_YAML" "$CELL_DIR/$CELL_YAML"

# 4. re-sync the pio tree copies (what `make -C firmware oil-sync` does)
mkdir -p "$FW/src/oil_pressure"
cp -f "$CELL_DIR"/tower_oil_pressure_port.c \
      "$CELL_DIR"/tower_port.h \
      "$CELL_DIR"/twin_config.h \
      "$CELL_DIR"/twin_snap.h \
      "$CELL_DIR"/twin_snap.cpp \
      "$CELL_DIR"/oil_pressure_main.cpp \
      "$FW/src/oil_pressure/"

# 5. receipts -- the lockstep fingerprints (cell sha matches the
#    provenance line quoted in the generated C header)
sha256sum "$CELL_DIR/$CELL_YAML" "$CELL_DIR/tower_oil_pressure_port.c"

echo "tower-sync: OK -- cells/oil-pressure/ refreshed in lockstep"
echo "tower-sync: next: make -C $FW oil-run   (host golden-vector gate)"
