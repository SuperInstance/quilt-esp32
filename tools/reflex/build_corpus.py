#!/usr/bin/env python3
# quilt-esp32 — tools/reflex/build_corpus.py
# REFLEX-ARC CORPUS BUILDER. Assembles the REAL critique vectors the
# desktop gate judged: every @piano bar the organism ever wrote, harvested
# from cell-cascade runs — final accepted bars (runs/*/[name].song) and
# candidate bars from the tick-log's compose/arrange answer heads — each
# re-measured through the SAME deterministic ear the runs used
# (plainsong analyze_features: parse → arrange → extract, voice=piano).
#
# Per-bar features are strictly bar-local in plainsong (onsets and
# interval steps inside the bar window only), so measuring a bar in a
# one-bar score yields byte-identical numbers to measuring it inside the
# run's full context. That assumption is not trusted: every one of the
# tick-log's `gate` evidence readings (channel · value · band, logged the
# moment the desktop gate judged them) is checked against the corpus —
# the anchors below prove the reconstruction.
#
# Run with the plainsong venv python (the same interpreter the MCP used):
#   /home/eileen/projects/plainsong-mcp/.venv/bin/python3 tools/reflex/build_corpus.py \
#       --runs /home/eileen/projects/cell-cascade/runs --out tools/reflex/vectors.jsonl
#
# Output vectors.jsonl, one line per distinct real bar:
#   {"id":N,"bar":"@piano | …","runs":[…],"sources":["song","head"],
#    "features":{"note_density":<µ>, … six channels, integer micro-units}}
# and vectors-meta.json next to it (counts + anchor validation).
import argparse
import glob
import json
import math
import os
import re
import sys

BAR_LINE = re.compile(r"^@[A-Za-z][A-Za-z0-9 _-]*\|[^|]*\|[^|]*$")
NORMALIZE = re.compile(r"^@[A-Za-z0-9 _-]*\|([^|]*\|[^|]*)$")
CHANNELS = [
    "note_density", "syncopation", "register_spread",
    "rest_ratio", "harmonic_tension", "interval_size",
]


def load_ear():
    from plainsong.notation import parse, arrange  # noqa: F401
    import plainsong.features as feat

    def measure(score_text: str) -> dict | None:
        score = parse(score_text)
        if score.has_errors:
            return None
        arrangement = arrange(score)
        found = feat.extract(arrangement, voice="piano")
        if not found:
            return None
        return dict(found[0].values)

    return measure


def song_header_meta(song_path: str) -> dict:
    meta = {"key": "C", "tempo": 100, "swing": "0%", "subdivision": "8th", "time": "4/4"}
    try:
        for line in open(song_path, encoding="utf-8"):
            s = line.strip()
            if s.startswith("key:"):
                for part in s.split("|"):
                    if ":" in part:
                        k, _, v = part.partition(":")
                        k, v = k.strip(), v.strip()
                        if k == "key":
                            meta["key"] = v
                        elif k == "tempo":
                            meta["tempo"] = v
                        elif k == "swing":
                            meta["swing"] = v
                        elif k == "subdivision":
                            meta["subdivision"] = v
            elif s.startswith("time:"):
                meta["time"] = s.split(":", 1)[1].strip()
            elif s.startswith("[") and not s.startswith("[MetaData]"):
                break
    except OSError:
        pass
    return meta


def assemble_score(meta: dict, bar: str) -> str:
    # byte-equivalent to cell-cascade assembleScore (src/cortex.ts)
    return "\n".join([
        "**TRACK: reflex-replay",
        "[MetaData]",
        f"key: {meta['key']} | tempo: {meta['tempo']} | swing: {meta['swing']} | subdivision: {meta['subdivision']}",
        f"time: {meta['time']}",
        "",
        "[A] (what the organism wrote — no human in the loop)",
        bar,
    ]) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", required=True, help="cell-cascade runs/ directory")
    ap.add_argument("--out", required=True, help="output vectors.jsonl")
    ap.add_argument("--stock", default=None,
                    help="stock-bars.json from cell-cascade scripts/reflex_stock_bars.ts "
                         "(arranger stock bars reconstructed through the real stockBarFor)")
    args = ap.parse_args()

    measure = load_ear()

    bars: dict[str, dict] = {}          # normalized bar line -> entry
    anchors: list[dict] = []            # tick-log gate evidence readings

    run_dirs = sorted(glob.glob(os.path.join(args.runs, "cortex-plug-*")))
    if not run_dirs:
        print(f"no cortex-plug-* runs under {args.runs}", file=sys.stderr)
        return 1

    def add_bar(bar: str, run: str, source: str) -> None:
        m = NORMALIZE.match(bar.strip())
        norm = f"@piano | {m.group(1).strip()}" if m else bar.strip()
        e = bars.setdefault(norm, {"bar": norm, "runs": [], "sources": []})
        if run not in e["runs"]:
            e["runs"].append(run)
        if source not in e["sources"]:
            e["sources"].append(source)

    for rd in run_dirs:
        run = os.path.basename(rd)
        songs = glob.glob(os.path.join(rd, "*.song"))
        meta = song_header_meta(songs[0]) if songs else {
            "key": "C", "tempo": 100, "swing": "0%", "subdivision": "8th", "time": "4/4"}

        # (a) final accepted bars — the gate judged the piano voice only
        for song in songs:
            for line in open(song, encoding="utf-8"):
                s = line.strip()
                if BAR_LINE.match(s) and s.startswith("@piano"):
                    add_bar(s, run, "song")

        # (b) candidate bars from answer heads (compose + escalated arrange)
        log = os.path.join(rd, "tick-log.jsonl")
        if os.path.exists(log):
            for line in open(log, encoding="utf-8"):
                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    continue
                head = d.get("answer_head")
                if isinstance(head, str):
                    for raw in head.splitlines():
                        if BAR_LINE.match(raw.strip()):
                            add_bar(raw, run, "head")
                if d.get("gate") and isinstance(d.get("evidence"), list):
                    for ev in d["evidence"]:
                        if ev.get("channel") in CHANNELS and isinstance(ev.get("value"), (int, float)):
                            anchors.append({
                                "run": run, "tick": d.get("tick"), "round": d.get("round"),
                                "channel": ev["channel"], "value": ev["value"],
                                "target_lo": ev.get("target_lo"), "target_hi": ev.get("target_hi"),
                                "severity": ev.get("severity"), "judged": ev.get("judged"),
                            })

    # (c) arranger stock bars — reconstructed by the real stockBarFor on the
    #     cell-cascade side (core-drift guarded there); measured here
    if args.stock and os.path.exists(args.stock):
        for s in json.load(open(args.stock, encoding="utf-8")).get("bars", []):
            add_bar(s["bar"], s["run"], "stock")

    # measure every distinct bar (sorted for deterministic ids)
    vectors = []
    skipped = []
    for i, norm in enumerate(sorted(bars)):
        e = bars[norm]
        run0 = e["runs"][0]
        rd = os.path.join(args.runs, run0)
        songs = glob.glob(os.path.join(rd, "*.song"))
        meta = song_header_meta(songs[0]) if songs else {
            "key": "C", "tempo": 100, "swing": "0%", "subdivision": "8th", "time": "4/4"}
        vals = measure(assemble_score(meta, norm))
        if vals is None:
            skipped.append({"bar": norm, "why": "notation errors"})
            continue
        feats = {}
        bad = False
        for ch in CHANNELS:
            v = vals.get(ch)
            if v is None:
                bad = True
                break
            u = round(v * 1_000_000)
            if abs(u / 1_000_000 - v) > 1e-9:  # off the µ grid (6dp guaranteed by the ear)
                bad = True
                break
            feats[ch] = u
        if bad:
            skipped.append({"bar": norm, "why": "missing channel / off-grid"})
            continue
        vectors.append({"id": len(vectors), "bar": norm, "runs": e["runs"],
                        "sources": e["sources"], "features": feats})

    # anchor validation: every logged evidence value must appear in the corpus.
    # The driver's evidence lines round values to 3dp (mint.ts r3) — the gate
    # judged the full 6dp trace, the ledger just abbreviated it — so anchors
    # compare at the logged precision. Matched anchors become single-channel
    # PROBES for the replay: the literal logged readings, fed to the board.
    corpus_by_run_channel = {}
    for vec in vectors:
        for run in vec["runs"]:
            for ch, u in vec["features"].items():
                corpus_by_run_channel.setdefault((run, ch), set()).add((math.floor(u / 1000 + 0.5), vec["id"], u))
    matched, unmatched, probes = 0, [], []
    for i, a in enumerate(anchors):
        want = math.floor(a["value"] * 1000 + 0.5)   # JS Math.round semantics — the driver's r3
        hits = [h for h in corpus_by_run_channel.get((a["run"], a["channel"]), set()) if h[0] == want]
        if hits:
            matched += 1
            _, vec_id, u = sorted(hits)[0]
            probes.append({"a": len(probes), "vec": vec_id, "ch": CHANNELS.index(a["channel"]),
                           "u": u, "run": a["run"], "tick": a["tick"], "round": a.get("round"),
                           "channel": a["channel"], "logged_3dp": a["value"],
                           "judged": a.get("judged"), "severity": a.get("severity")})
        else:
            unmatched.append(a)

    meta_out = {
        "runs_scanned": len(run_dirs),
        "distinct_real_bars": len(bars),
        "vectors": len(vectors),
        "channel_readings": len(vectors) * 6,
        "skipped": skipped,
        "anchors": {"evidence_readings": len(anchors), "matched": matched,
                    "unmatched": unmatched, "probes": len(probes)},
        "provenance": "every @piano bar from runs/*/*.song + tick-log answer heads, "
                      "re-measured by plainsong analyze_features (voice=piano), µ = 1e-6",
    }
    with open(args.out, "w", encoding="utf-8") as f:
        for vec in vectors:
            f.write(json.dumps(vec, ensure_ascii=False) + "\n")
    # plain feed for the C host harness + board: id + six µ readings in
    # frozen channel order
    with open(os.path.splitext(args.out)[0] + ".txt", "w", encoding="utf-8") as f:
        for vec in vectors:
            f.write(str(vec["id"]) + " " + " ".join(str(vec["features"][ch]) for ch in CHANNELS) + "\n")
    # anchor probes (the literal logged readings, matched to their corpus vector)
    with open(os.path.splitext(args.out)[0] + "-anchors.jsonl", "w", encoding="utf-8") as f:
        for p in probes:
            f.write(json.dumps(p, ensure_ascii=False) + "\n")
    with open(os.path.splitext(args.out)[0] + "-meta.json", "w", encoding="utf-8") as f:
        json.dump(meta_out, f, indent=1, ensure_ascii=False)

    print(f"corpus: {len(vectors)} vectors ({len(vectors) * 6} channel readings) "
          f"from {len(bars)} distinct real bars across {len(run_dirs)} runs")
    print(f"anchors: {matched}/{len(anchors)} logged gate-evidence readings reproduced exactly"
          + (f" — UNMATCHED: {len(unmatched)}" if unmatched else ""))
    if skipped:
        print(f"skipped {len(skipped)} bars: {skipped[:3]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
