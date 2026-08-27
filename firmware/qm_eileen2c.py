#!/usr/bin/env python3
# quilt-esp32 — firmware/qm_eileen2c.py
# Compile eileen.qm (THE EILEEN, the metal resolution) into eileen_qm.h C
# tables. Doctrine mirrors qm_vessel2c.py: the .qm is authored directly in
# this repo (no upstream mint), so the receipt seals the file itself:
#
#   receipt = sha256( file bytes with the 64-hex source_sha256 value
#                     replaced by 64 '0' characters )
#
# `--mint` seals the receipt into the .qm in place (idempotent); compile
# mode verifies it, then validates the vessel against the frozen canon
# (ten named pieces, the manifest's joint order, the steel sheet's
# formulas — cross-resolution agreement is the acceptance test, enforced
# here at compile time and re-verified row by row by the host harness).
#
#   qm_eileen2c.py --mint eileen.qm
#   qm_eileen2c.py eileen.qm src/eileen/eileen_qm.h
import hashlib
import json
import re
import sys

SHA_RE = re.compile(r"^[0-9a-f]{64}$")
ZERO = "0" * 64

# the frozen vessel: cell names in the manifest's joint order, the steel
# sheet's formulas (eileen.sheet.yaml exprs, transcribed), read sets
CANON_CELLS = [
    "keel", "stem", "keelson", "breast_hook", "rigging",
    "bulwarks", "ensign", "scuppers", "sheerboard", "figurehead",
]
CANON_FORMULAS = {
    "keelson": "stem + keel",
    "breast_hook": "keelson * 2",
    "rigging": "breast_hook + keelson",
    "bulwarks": "rigging - 2",
    "ensign": "bulwarks + 1",
    "scuppers": "ensign - 1",
    "sheerboard": "scuppers + breast_hook",
    "figurehead": "sheerboard > 0 ? keel : 0",
}
CANON_READS = {
    "keel": [],
    "stem": ["uart"],
    "keelson": ["stem", "keel"],
    "breast_hook": ["keelson"],
    "rigging": ["breast_hook", "keelson"],
    "bulwarks": ["rigging"],
    "ensign": ["bulwarks"],
    "scuppers": ["ensign"],
    "sheerboard": ["scuppers", "breast_hook"],
    "figurehead": ["sheerboard"],
}
CANON_LOG = "she faces forward; the days grew from the keel"


def die(msg: str) -> None:
    print(f"qm_eileen2c: {msg}", file=sys.stderr)
    sys.exit(1)


def receipt_of(raw: bytes, receipt: str) -> str:
    """sha256 of the file bytes with the receipt value zeroed."""
    return hashlib.sha256(raw.replace(receipt.encode(), ZERO.encode())).hexdigest()


def load(qm_path: str):
    raw = open(qm_path, "rb").read()
    prog = json.loads(raw.decode("utf-8"))
    if prog.get("format") != "qm" or prog.get("version") != 1:
        die("not a .qm v1 program")
    if prog.get("organism") != "eileen-limb":
        die(f"wrong organism: {prog.get('organism')}")
    facts = None
    for op in prog["ops"]:
        if op.get("op") == "bind" and op.get("target") == "eileen-limb:facts":
            facts = op["value"]
    if facts is None:
        die("no eileen-limb:facts bind")
    return raw, facts


def mint(qm_path: str) -> None:
    raw, facts = load(qm_path)
    sha = facts.get("source_sha256", "")
    if not SHA_RE.match(sha):
        die(f"bad receipt field: {sha!r}")
    new = receipt_of(raw, sha)
    if sha == new:
        print(f"mint: receipt already sealed ({new[:16]}…)")
        return
    out = raw.replace(sha.encode(), new.encode())
    open(qm_path, "wb").write(out)
    print(f"mint: sealed receipt {new[:16]}… into {qm_path}")


def is_int(v) -> bool:
    return isinstance(v, int) and not isinstance(v, bool)


def main() -> None:
    if len(sys.argv) == 3 and sys.argv[1] == "--mint":
        mint(sys.argv[2])
        return
    if len(sys.argv) != 3:
        die("usage: qm_eileen2c.py <eileen.qm> <out eileen_qm.h> | --mint <qm>")
    qm_path, out_path = sys.argv[1], sys.argv[2]
    raw, facts = load(qm_path)

    # receipt
    sha = facts.get("source_sha256", "")
    if not SHA_RE.match(sha):
        die(f"bad receipt field: {sha!r} (run --mint first)")
    if receipt_of(raw, sha) != sha:
        die("receipt does not verify: file changed since mint (re-run --mint to re-seal)")

    # the vessel: ten pieces in the manifest's joint order
    cells = [c["name"] for c in facts.get("cells", [])]
    if cells != CANON_CELLS:
        die(f"cells must be the ten pieces in joint order, got {cells}")
    if facts.get("joint_order") != CANON_CELLS:
        die("joint_order must equal the cell order (the manifest chain)")
    if facts.get("formulas") != CANON_FORMULAS:
        die(f"formulas must match the steel sheet exactly, got {facts.get('formulas')}")
    if facts.get("read_sets") != CANON_READS:
        die("read_sets must match the steel sheet's dependency edges")

    # every read set is predecessors-only: each piece reads the ones
    # before it, in order — the manifest's joints
    for i, name in enumerate(CANON_CELLS):
        allowed = set(CANON_CELLS[:i]) | {"uart"}
        for dep in CANON_READS[name]:
            if dep not in allowed:
                die(f"{name} reads {dep}, which is not a predecessor (joint order broken)")

    # the keel: value 1, the blink, half-second on / half-second off
    keel = facts.get("keel", {})
    if not (is_int(keel.get("value")) and keel["value"] == 1):
        die("keel.value must be 1")
    if keel.get("tempo_bpm") != 60 or keel.get("blink_ms") != 500:
        die("keel must beat ♩=60 (500 ms on, 500 ms off)")
    if keel.get("led", "").find("13:19") < 0:
        die("keel.led must cite the 13:19 blink (blink.qm, movement I)")

    # the figurehead: watches the sheerboard, closes on the keel
    fh = facts.get("figurehead", {})
    if (fh.get("watch") != "sheerboard" or fh.get("closes_on") != "keel"
            or fh.get("condition") != "sheerboard > 0"
            or fh.get("log") != CANON_LOG):
        die("figurehead must watch sheerboard, close on keel, carry the log line")

    # integer discipline: the closed form and the day ceiling
    disc = facts.get("integer_discipline", {})
    if disc.get("day_max") != (2**31 - 1 - 3) // 5:
        die("integer_discipline.day_max must be (INT32_MAX-3)/5")

    L = []
    a = L.append
    a(f"/* generated by qm_eileen2c.py from {qm_path} — do not edit */")
    a(f"/* THE EILEEN, metal resolution · rules v{facts.get('rule_version')} · exported {facts.get('exported_at')} */")
    a(f"/* materials: {facts['materials']['prose']} + {facts['materials']['steel']} + {facts['materials']['bread']} + this */")
    a("#ifndef EILEEN_QM_H")
    a("#define EILEEN_QM_H")
    a("")
    a("#include <stdint.h>")
    a("")
    a(f'#define EILEEN_QM_SHA256 "{sha}"')
    a(f"#define EILEEN_QM_VERSION {facts.get('rule_version')}")
    a("")
    a("/* frozen piece order (joint_order = the manifest's joint chain) */")
    idx = ["KEEL", "STEM", "KEELSON", "BREAST_HOOK", "RIGGING",
           "BULWARKS", "ENSIGN", "SCUPPERS", "SHEERBOARD", "FIGUREHEAD"]
    for i, c in enumerate(CANON_CELLS):
        a(f"#define EILEEN_CELL_{idx[i]} {i}")
    a(f"#define EILEEN_QM_N_CELLS {len(CANON_CELLS)}")
    a("")
    a("/* the keel: value 1, the 2026-08-26 13:19 blink (blink.qm,")
    a(" * movement I of the bread score). ♩=60: 500 ms lit, 500 ms dark. */")
    a("#define EILEEN_KEEL_VALUE 1")
    a("#define EILEEN_KEEL_BLINK_MS 500")
    a("")
    a("/* the joint chain, compile-time frozen — the manifest's order */")
    a("static const uint8_t EILEEN_QM_JOINTS[EILEEN_QM_N_CELLS] = {")
    a("    " + ", ".join(str(i) for i in range(len(CANON_CELLS))) + ", /* keel..figurehead */")
    a("};")
    a("")
    a("/* the figurehead's closing line (log.figurehead) */")
    a(f'#define EILEEN_QM_LOG "{CANON_LOG}"')
    a("")
    a("/* integer discipline: day saturates so sheerboard=5N+3 fits int32 */")
    a(f"#define EILEEN_QM_DAY_MAX {disc['day_max']}")
    a("")
    a("#endif /* EILEEN_QM_H */")
    open(out_path, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print(f"wrote {out_path}: vessel v{facts.get('rule_version')} sha {sha[:16]}… "
          f"10 pieces joint keel->figurehead, keel=1 blink 500ms, "
          f"figurehead closes on keel")


if __name__ == "__main__":
    main()
