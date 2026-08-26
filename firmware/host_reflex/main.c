/* host_reflex/main.c — the metal replay, run on the desktop.
 *
 * Compiles the SAME critic_gate.c the ESP32-S3 firmware runs and replays
 * the real critique corpus (vectors.txt: id + six µ readings, frozen
 * channel order) plus the ledger anchor probes (anchors file: a vec ch u),
 * comparing every judgment against the desktop gate's reference
 * (ref.txt: id sev×6 gray×6 pen verdict — produced by cell-cascade
 * scripts/reflex_reference.ts through the REAL cheapCritique).
 *
 * ACCEPTANCE: 100% agreement. Any divergence is printed and recorded as a
 * pre-registered finding (table inexpressiveness / band-edge rounding) in
 * findings.json — divergences are results, not embarrassments.
 *
 * Latency: each vector is judged twice — once cold (measurement, ns via
 * clock_gettime) and once for the record — the histogram (p50/p99 per
 * verdict, µs) approximates what the board's esp_timer µs stamp will show.
 *
 *   ./host_reflex_bin vectors.txt ref.txt vectors-anchors.txt findings.json
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "critic_gate.h"

#define MAXV 4096
#define MAXA 256

typedef struct {
    int id;
    long long f[6];
} vec_t;

typedef struct {
    int id, sev[6], gray[6], pen, verdict;
} ref_t;

typedef struct {
    int a, vec, ch;
    long long u;
} probe_t;

static int cmp_ll(const void *x, const void *y)
{
    long long a = *(const long long *)x, b = *(const long long *)y;
    return a < b ? -1 : a > b ? 1 : 0;
}

static long long pct(long long *v, int n, int p)
{
    if (n == 0) return 0;
    int i = (n - 1) * p / 100;
    return v[i];
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s vectors.txt ref.txt anchors.txt findings.json\n", argv[0]);
        return 2;
    }

    static vec_t vecs[MAXV];
    static ref_t refs[MAXV];
    static probe_t probes[MAXA];
    int nv = 0, nr = 0, na = 0;

    FILE *fv = fopen(argv[1], "r");
    if (!fv) { perror(argv[1]); return 2; }
    while (nv < MAXV && fscanf(fv, "%d %lld %lld %lld %lld %lld %lld",
                               &vecs[nv].id, &vecs[nv].f[0], &vecs[nv].f[1],
                               &vecs[nv].f[2], &vecs[nv].f[3], &vecs[nv].f[4],
                               &vecs[nv].f[5]) == 7) nv++;
    fclose(fv);

    FILE *fr = fopen(argv[2], "r");
    if (!fr) { perror(argv[2]); return 2; }
    while (nr < MAXV && fscanf(fr, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                               &refs[nr].id, &refs[nr].sev[0], &refs[nr].sev[1],
                               &refs[nr].sev[2], &refs[nr].sev[3], &refs[nr].sev[4],
                               &refs[nr].sev[5], &refs[nr].gray[0], &refs[nr].gray[1],
                               &refs[nr].gray[2], &refs[nr].gray[3], &refs[nr].gray[4],
                               &refs[nr].gray[5], &refs[nr].pen,
                               &refs[nr].verdict) == 15) nr++;
    fclose(fr);

    FILE *fa = fopen(argv[3], "r");
    if (fa) {
        char line[512];
        while (na < MAXA && fgets(line, sizeof line, fa)) {
            probe_t p;
            if (sscanf(line, "{\"a\": %d, \"vec\": %d, \"ch\": %d, \"u\": %lld",
                       &p.a, &p.vec, &p.ch, &p.u) == 4)
                probes[na++] = p;
        }
        fclose(fa);
    }

    if (nv != nr) {
        fprintf(stderr, "corpus/reference mismatch: %d vectors vs %d refs\n", nv, nr);
        return 2;
    }

    long long lat_acc[MAXV], lat_rev[MAXV];
    int n_acc = 0, n_rev = 0;
    long long read_ok = 0, read_bad = 0;
    long long bar_ok = 0, bar_bad = 0;
    long long probe_ok = 0, probe_bad = 0;
    FILE *ff = fopen(argv[4], "w");
    FILE *dl = fopen("dissent-ledger-host.jsonl", "w");

    fprintf(ff, "{\n \"pre_registered\": [\"table inexpressiveness\", \"band-edge rounding\"],\n");
    fprintf(ff, " \"divergences\": [\n");
    int ndiv = 0;

    for (int i = 0; i < nv; i++) {
        int32_t f[6];
        for (int c = 0; c < 6; c++) f[c] = (int32_t)vecs[i].f[c];

        /* cold measurement for the histogram (ns — the judge is sub-µs) */
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        gate_judgment_t j;
        critic_gate_judge(f, &j);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long long ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
        if (j.verdict == GATE_VERDICT_REVISE) lat_rev[n_rev++] = ns;
        else lat_acc[n_acc++] = ns;

        /* compare against the desktop reference */
        for (int c = 0; c < 6; c++) {
            if (j.sev[c] == refs[i].sev[c]) read_ok++;
            else {
                read_bad++;
                printf("DIVERGENCE vec %d ch %d (%s): metal %d desktop %d (v=%ld)\n",
                       vecs[i].id, c, gate_qm_bands[c].name, j.sev[c], refs[i].sev[c],
                       (long)f[c]);
                fprintf(ff, "%s  {\"kind\":\"reading\",\"vec\":%d,\"ch\":%d,\"channel\":\"%s\","
                       "\"value_u\":%ld,\"metal_sev\":%d,\"desktop_sev\":%d}",
                       ndiv++ ? "," : "", vecs[i].id, c, gate_qm_bands[c].name,
                       (long)f[c], j.sev[c], refs[i].sev[c]);
            }
        }
        int gray_match = 1;
        for (int c = 0; c < 6; c++)
            if (((j.gray >> c) & 1) != refs[i].gray[c]) gray_match = 0;
        if (j.penalty_u != refs[i].pen || j.verdict != refs[i].verdict || !gray_match) {
            bar_bad++;
            printf("DIVERGENCE vec %d: metal pen=%ld verdict=%d gray=%d vs desktop pen=%d verdict=%d\n",
                   vecs[i].id, (long)j.penalty_u, (int)j.verdict, (int)j.gray,
                   refs[i].pen, refs[i].verdict);
            fprintf(ff, "%s  {\"kind\":\"bar\",\"vec\":%d,\"metal\":{\"pen\":%ld,\"verdict\":%d,\"gray\":%d},"
                   "\"desktop\":{\"pen\":%d,\"verdict\":%d}}",
                   ndiv++ ? "," : "", vecs[i].id, (long)j.penalty_u, (int)j.verdict,
                   (int)j.gray, refs[i].pen, refs[i].verdict);
        } else bar_ok++;

        /* dissent ledger (stretch): near-edge readings, metal's own flags */
        for (int c = 0; c < 6; c++) {
            if (j.dissent & (1u << c)) {
                int32_t lo = gate_qm_bands[c].lo, hi = gate_qm_bands[c].hi;
                int32_t dlo = f[c] - lo; if (dlo < 0) dlo = -dlo;
                int32_t dhi = f[c] - hi; if (dhi < 0) dhi = -dhi;
                int32_t edge = dlo <= dhi ? lo : hi;
                fprintf(dl, "{\"vec\":%d,\"ch\":%d,\"channel\":\"%s\",\"value_u\":%ld,"
                       "\"edge_u\":%ld,\"dist_u\":%ld}\n",
                       vecs[i].id, c, gate_qm_bands[c].name, (long)f[c],
                       (long)edge, (long)(dlo <= dhi ? dlo : dhi));
            }
        }
    }

    /* anchor probes: the literal logged ledger readings */
    for (int i = 0; i < na; i++) {
        int32_t sev; int gray, dissent;
        critic_gate_judge_channel(probes[i].ch, (int32_t)probes[i].u, &sev, &gray, &dissent);
        /* expected severity lives in the matched vector's reference line */
        int exp = refs[probes[i].vec].sev[probes[i].ch];
        int exp_gray = refs[probes[i].vec].gray[probes[i].ch];
        if (sev == exp && gray == exp_gray) probe_ok++;
        else {
            probe_bad++;
            printf("DIVERGENCE anchor %d (vec %d ch %d): metal %d/%d desktop %d/%d\n",
                   probes[i].a, probes[i].vec, probes[i].ch, sev, gray, exp, exp_gray);
            fprintf(ff, "%s  {\"kind\":\"anchor\",\"anchor\":%d,\"vec\":%d,\"ch\":%d,"
                   "\"value_u\":%ld,\"metal_sev\":%d,\"desktop_sev\":%d}",
                   ndiv++ ? "," : "", probes[i].a, probes[i].vec, probes[i].ch,
                   (long)probes[i].u, sev, exp);
        }
    }

    fprintf(ff, "\n ],\n");
    long long readings = read_ok + read_bad;
    long long bars = bar_ok + bar_bad;
    long long probes_t = probe_ok + probe_bad;
    fprintf(ff, " \"agreement\": {\n");
    fprintf(ff, "  \"channel_readings\": {\"ok\": %lld, \"total\": %lld, \"pct\": %.4f},\n",
            read_ok, readings, readings ? 100.0 * read_ok / readings : 0.0);
    fprintf(ff, "  \"bar_verdicts\": {\"ok\": %lld, \"total\": %lld, \"pct\": %.4f},\n",
            bar_ok, bars, bars ? 100.0 * bar_ok / bars : 0.0);
    fprintf(ff, "  \"anchor_probes\": {\"ok\": %lld, \"total\": %lld, \"pct\": %.4f}\n",
            probe_ok, probes_t, probes_t ? 100.0 * probe_ok / probes_t : 0.0);
    fprintf(ff, " },\n");
    fprintf(ff, " \"latency_ns_desktop\": {\n");   /* board stamps µs (esp_timer) — see report */
    qsort(lat_acc, n_acc, sizeof(long long), cmp_ll);
    qsort(lat_rev, n_rev, sizeof(long long), cmp_ll);
    fprintf(ff, "  \"accept\": {\"n\": %d, \"p50\": %lld, \"p99\": %lld},\n",
            n_acc, pct(lat_acc, n_acc, 50), pct(lat_acc, n_acc, 99));
    fprintf(ff, "  \"revise\": {\"n\": %d, \"p50\": %lld, \"p99\": %lld}\n",
            n_rev, pct(lat_rev, n_rev, 50), pct(lat_rev, n_rev, 99));
    fprintf(ff, " },\n");
    fprintf(ff, " \"gate\": {\"version\": %d, \"sha256\": \"%s\", \"bands_sha_note\": \"see critic-gate.qm\"}\n",
            GATE_QM_VERSION, GATE_QM_SHA256);
    fprintf(ff, "}\n");
    fclose(ff);
    fclose(dl);

    printf("replay: %lld channel readings — %lld agree (%.4f%%)\n",
           readings, read_ok, readings ? 100.0 * read_ok / readings : 0.0);
    printf("        %lld bar verdicts — %lld agree (%.4f%%)\n",
           bars, bar_ok, bars ? 100.0 * bar_ok / bars : 0.0);
    printf("        %lld anchor probes — %lld agree (%.4f%%)\n",
           probes_t, probe_ok, probes_t ? 100.0 * probe_ok / probes_t : 0.0);
    printf("latency ns (desktop, -O2): accept p50=%lld p99=%lld (n=%d) · revise p50=%lld p99=%lld (n=%d)\n",
           pct(lat_acc, n_acc, 50), pct(lat_acc, n_acc, 99), n_acc,
           pct(lat_rev, n_rev, 50), pct(lat_rev, n_rev, 99), n_rev);
    printf("findings → %s · dissent ledger → dissent-ledger-host.jsonl\n", argv[4]);

    return (read_bad == 0 && bar_bad == 0 && probe_bad == 0) ? 0 : 1;
}
