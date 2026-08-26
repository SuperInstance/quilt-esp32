/* critic_gate.c — the frozen 6-channel band gate in integer micro-units.
 * Portable C99: compiled unchanged into the ESP32-S3 firmware and the
 * host replay harness — the replay proves THIS code identical to the
 * desktop gate. Integer discipline everywhere; no floats on target. */
#include "critic_gate.h"

#include <stddef.h>

static int32_t clamp_to_edge_dist(int32_t v, int32_t lo, int32_t hi,
                                  int32_t *edge_out)
{
    int32_t dlo = v - lo; if (dlo < 0) dlo = -dlo;
    int32_t dhi = v - hi; if (dhi < 0) dhi = -dhi;
    if (dlo <= dhi) { *edge_out = lo; return dlo; }
    *edge_out = hi; return dhi;
}

static void judge_channel(int ch_idx, int32_t v,
                          int32_t *sev, int *gray, int *dissent,
                          int32_t *edge, int32_t *dist)
{
    const int32_t lo = gate_qm_bands[ch_idx].lo;
    const int32_t hi = gate_qm_bands[ch_idx].hi;
    const int32_t amb = GATE_QM_AMBIGUITY;

    if (v < lo - amb || v > hi + amb) {
        *sev = GATE_SEV_BAD;   /* clear violation — past the gray zone */
        *gray = 0;
    } else if (v < lo || v > hi) {
        *sev = GATE_SEV_WARN;  /* the gray zone — seam territory */
        *gray = 1;
    } else {
        *sev = GATE_SEV_OK;    /* inclusive edges: v == lo/v == hi is ok */
        *gray = 0;
    }

    /* dissent (stretch seed): a reading near ANY band edge — inside the
     * band but within ε of an edge, or in the gray zone — is flagged back
     * for the escalation ledger. Warn readings are within amb of an edge
     * by construction; they always dissent. CLEAR violations do not:
     * dissent is the ambiguity embryo, and a far-past-edge bad is not
     * ambiguous. */
    *dist = clamp_to_edge_dist(v, lo, hi, edge);
    *dissent = (*dist <= GATE_QM_DISSENT_EPS || *gray) ? 1 : 0;
}

void critic_gate_judge_channel(int ch_idx, int32_t value_u,
                               int32_t *sev, int *gray, int *dissent)
{
    int32_t edge = 0, dist = 0;
    if (ch_idx < 0 || ch_idx >= GATE_QM_N_CHANNELS) { *sev = -1; *gray = 0; *dissent = 0; return; }
    judge_channel(ch_idx, value_u, sev, gray, dissent, &edge, &dist);
}

void critic_gate_judge(const int32_t features_u[GATE_QM_N_CHANNELS],
                       gate_judgment_t *out)
{
    int32_t penalty = 0;
    int32_t worst = GATE_SEV_OK;
    out->gray = 0;
    out->dissent = 0;

    for (int i = 0; i < GATE_QM_N_CHANNELS; i++) {
        int32_t sev, gray, dissent, edge, dist;
        judge_channel(i, features_u[i], &sev, &gray, &dissent, &edge, &dist);
        out->sev[i] = sev;
        if (gray)   out->gray   |= (uint8_t)(1u << i);
        if (dissent) out->dissent |= (uint8_t)(1u << i);
        penalty += sev == GATE_SEV_BAD ? GATE_QM_PENALTY_BAD
                : sev == GATE_SEV_WARN ? GATE_QM_PENALTY_WARN : 0;
        if (sev > worst) worst = sev;
    }

    out->penalty_u = penalty;
    out->verdict = penalty >= GATE_QM_REVISE_THRESHOLD
                   ? GATE_VERDICT_REVISE : GATE_VERDICT_ACCEPT;
    out->worst = worst;
}

const char *critic_gate_sha256(void) { return GATE_QM_SHA256; }
