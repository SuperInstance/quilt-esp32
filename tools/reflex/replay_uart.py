#!/usr/bin/env python3
# quilt-esp32 — tools/reflex/replay_uart.py
# THE BOARD REPLAY. Feeds the real critique corpus + ledger anchors to the
# reflex-arc firmware over UART, verifies the boot mint-receipt sha256,
# compares every answer to the desktop reference (cell-cascade
# reflex_reference.ts through the REAL cheapCritique), and writes the
# acceptance report: agreement %, latency histogram (board µs stamps),
# divergence findings, and the dissent ledger.
#
#   python3 tools/reflex/replay_uart.py --port /dev/ttyACM0 \
#       --corpus tools/reflex/vectors.txt --ref tools/reflex/ref.txt \
#       --anchors tools/reflex/vectors-anchors.jsonl \
#       --qm firmware/critic-gate.qm --out-dir tools/reflex/board
#
# Requires pyserial. The board must be running the reflex_arc firmware
# (see docs/REFLEX-ARC-2026-08-26.md for the flash line).
import argparse
import json
import os
import re
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial missing: pip install pyserial", file=sys.stderr)
    sys.exit(2)

CHANNELS = ["note_density", "syncopation", "register_spread",
            "rest_ratio", "harmonic_tension", "interval_size"]


def read_line(ser, timeout=5.0):
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        while ser.in_waiting:
            b = ser.read(1)
            if b == b"\n":
                return buf.decode("utf-8", "replace").strip()
            buf += b
    return None


def qm_sha(path):
    qm = json.load(open(path, encoding="utf-8"))
    for op in qm["ops"]:
        if op.get("op") == "bind" and op.get("target") == "critic-gate:facts":
            return op["value"]["source_sha256"]
    raise SystemExit(f"no facts bind in {path}")


def pct(sorted_v, p):
    if not sorted_v:
        return None
    return sorted_v[min(len(sorted_v) - 1, (len(sorted_v) - 1) * p // 100)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--corpus", required=True, help="vectors.txt (id + 6 µ)")
    ap.add_argument("--ref", required=True, help="ref.txt (id sev×6 gray×6 pen verdict)")
    ap.add_argument("--anchors", required=True, help="vectors-anchors.jsonl")
    ap.add_argument("--qm", default="../firmware/critic-gate.qm", help="critic-gate.qm for sha check")
    ap.add_argument("--out-dir", default="board")
    args = ap.parse_args()

    want_sha = qm_sha(args.qm)

    vectors = []
    for line in open(args.corpus, encoding="utf-8"):
        parts = line.split()
        if len(parts) == 7:
            vectors.append((int(parts[0]), [int(x) for x in parts[1:]]))
    refs = {}
    for line in open(args.ref, encoding="utf-8"):
        parts = line.split()
        if len(parts) == 15:
            refs[int(parts[0])] = {
                "sev": [int(x) for x in parts[1:7]],
                "gray": [int(x) for x in parts[7:13]],
                "pen": int(parts[13]), "verdict": int(parts[14]),
            }
    anchors = []
    for line in open(args.anchors, encoding="utf-8"):
        anchors.append(json.loads(line))

    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(1.0)          # let the board finish booting
    ser.reset_input_buffer()
    ser.write(b"B\n")        # reprint the banner — the mint receipt
    time.sleep(0.5)

    banner_sha = None
    for _ in range(20):
        ln = read_line(ser, timeout=1.0)
        if ln is None:
            continue
        print(f"[board] {ln}")
        m = re.search(r"mint-receipt sha256: ([0-9a-f]{64})", ln)
        if m:
            banner_sha = m.group(1)
        if ln.startswith("ready"):
            break
    if banner_sha != want_sha:
        print(f"FATAL: board sha {banner_sha} != corpus sha {want_sha} — stale image?", file=sys.stderr)
        sys.exit(1)
    print(f"[host] mint receipt verified: {banner_sha}")

    os.makedirs(args.out_dir, exist_ok=True)
    divergences = []
    dissent_ledger = open(os.path.join(args.out_dir, "dissent-ledger-board.jsonl"), "w")
    lat = {"accept": [], "revise": []}
    read_ok = read_bad = bar_ok = bar_bad = probe_ok = probe_bad = 0
    t_start = time.time()

    for vid, f in vectors:
        ser.write(f"V {vid} {' '.join(str(x) for x in f)}\n".encode())
        r = read_line(ser)
        if not r or not r.startswith("R "):
            divergences.append({"kind": "protocol", "vec": vid, "line": r})
            bar_bad += 6
            read_bad += 6
            continue
        p = r.split()
        # R id s1..s6 gray dissent pen verdict us
        vid_b = int(p[1]); sev = [int(x) for x in p[2:8]]
        gray = int(p[8]); dissent = int(p[9])
        pen, verdict, us = int(p[10]), int(p[11]), int(p[12])
        assert vid_b == vid
        lat["revise" if verdict else "accept"].append(us)

        ref = refs[vid]
        for c in range(6):
            if sev[c] == ref["sev"][c] and ((gray >> c) & 1) == ref["gray"][c]:
                read_ok += 1
            else:
                read_bad += 1
                divergences.append({"kind": "reading", "vec": vid, "ch": c,
                                    "channel": CHANNELS[c], "value_u": f[c],
                                    "metal_sev": sev[c], "desktop_sev": ref["sev"][c],
                                    "metal_gray": (gray >> c) & 1, "desktop_gray": ref["gray"][c]})
        if pen == ref["pen"] and verdict == ref["verdict"]:
            bar_ok += 1
        else:
            bar_bad += 1
            divergences.append({"kind": "bar", "vec": vid, "metal": {"pen": pen, "verdict": verdict},
                                "desktop": {"pen": ref["pen"], "verdict": ref["verdict"]}})

        # dissent detail lines follow while flagged
        for _ in range(bin(dissent).count("1")):
            d = read_line(ser)
            if d and d.startswith("D "):
                dp = d.split()
                dissent_ledger.write(json.dumps({
                    "vec": int(dp[1]), "ch": int(dp[2]), "channel": CHANNELS[int(dp[2])],
                    "value_u": int(dp[3]), "edge_u": int(dp[4]), "dist_u": int(dp[5]),
                }) + "\n")

    for a in anchors:
        ser.write(f"P {a['a']} {a['ch']} {a['u']}\n".encode())
        q = read_line(ser)
        if not q or not q.startswith("Q "):
            probe_bad += 1
            divergences.append({"kind": "protocol", "anchor": a["a"], "line": q})
            continue
        p = q.split()
        sev, gray = int(p[3]), int(p[4])
        ref = refs[a["vec"]]
        if sev == ref["sev"][a["ch"]] and gray == ref["gray"][a["ch"]]:
            probe_ok += 1
        else:
            probe_bad += 1
            divergences.append({"kind": "anchor", "anchor": a["a"], "value_u": a["u"],
                                "metal_sev": sev, "desktop_sev": ref["sev"][a["ch"]]})
    wall_s = time.time() - t_start

    la = sorted(lat["accept"]); lr = sorted(lat["revise"])
    report = {
        "board": args.port,
        "mint_receipt_sha256": banner_sha,
        "agreement": {
            "channel_readings": {"ok": read_ok, "total": read_ok + read_bad,
                                 "pct": round(100.0 * read_ok / (read_ok + read_bad), 4) if read_ok + read_bad else 0},
            "bar_verdicts": {"ok": bar_ok, "total": bar_ok + bar_bad,
                             "pct": round(100.0 * bar_ok / (bar_ok + bar_bad), 4) if bar_ok + bar_bad else 0},
            "anchor_probes": {"ok": probe_ok, "total": probe_ok + probe_bad,
                              "pct": round(100.0 * probe_ok / (probe_ok + probe_bad), 4) if probe_ok + probe_bad else 0},
        },
        "latency_us_board": {
            "accept": {"n": len(la), "p50": pct(la, 50), "p99": pct(la, 99)},
            "revise": {"n": len(lr), "p50": pct(lr, 50), "p99": pct(lr, 99)},
        },
        "wall_seconds": round(wall_s, 1),
        "pre_registered": ["table inexpressiveness", "band-edge rounding"],
        "divergences": divergences,
    }
    with open(os.path.join(args.out_dir, "findings-board.json"), "w") as f:
        json.dump(report, f, indent=1)
    dissent_ledger.close()

    print(f"board replay: {read_ok + read_bad} readings — {read_ok} agree "
          f"({report['agreement']['channel_readings']['pct']:.4f}%)")
    print(f"             {bar_ok + bar_bad} verdicts — {bar_ok} agree "
          f"({report['agreement']['bar_verdicts']['pct']:.4f}%)")
    print(f"             {probe_ok + probe_bad} anchors — {probe_ok} agree "
          f"({report['agreement']['anchor_probes']['pct']:.4f}%)")
    print(f"board latency µs: accept p50={pct(la, 50)} p99={pct(la, 99)} · "
          f"revise p50={pct(lr, 50)} p99={pct(lr, 99)} · wall {wall_s:.1f}s")
    print(f"report → {args.out_dir}/findings-board.json · dissent → {args.out_dir}/dissent-ledger-board.jsonl")
    sys.exit(0 if not divergences else 1)


if __name__ == "__main__":
    main()
