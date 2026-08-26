/* critic_gate.h — the critic's frozen 6-channel band gate, integer-only.
 *
 * Reflex-arc lane (2026-08-26): the desktop gate (cell-cascade
 * src/critic.ts cheapCritique, band observations) exported to ESP32-S3 as
 * fixed-point micro-units. 1 µ = 1e-6, signed 32-bit — every number in the
 * pipeline (ear features rounded to 6dp, decimal bands, 0.06 gray zone,
 * 0.4/1.0 penalties) is EXACT on that grid; Q16.16 is not (0.06 and 0.763
 * are undyadic — ±2.4e-6 edge error). No float exists on this path, on
 * host or metal: host and firmware compile THIS file, so the replay
 * compares the same integer code the board runs.
 *
 * Semantics (mirror of cheapCritique band branch, per channel):
 *   bad  : v < lo - ambiguity  ||  v > hi + ambiguity   (clear violation)
 *   warn : else v < lo || v > hi                          (the gray zone)
 *   ok   : lo <= v <= hi                                   (inclusive edges)
 * penalty: ok 0 / warn 400000 / bad 1000000 µ; verdict revise when the
 * bar's total penalty >= 1000000 — the same threshold crossings as the
 * desktop float math (0.4, 0.8 < 1; 1.2, 1.4, ... >= 1; bad = 1.0 >= 1).
 *
 * Scope (pre-registered): the 6-channel band gate + gray zone only.
 * Cross-bar tissue (voice-leading, tension-curve, librettist arc) is
 * desktop concern — off-metal by design.
 */
#ifndef CRITIC_GATE_H
#define CRITIC_GATE_H

#include <stdint.h>

/* generated from critic-gate.qm (qm_gate2c.py) — the mint's artifact */
#include "gate_qm.h"

#define GATE_SEV_OK 0
#define GATE_SEV_WARN 1
#define GATE_SEV_BAD 2

#define GATE_VERDICT_ACCEPT 0
#define GATE_VERDICT_REVISE 1

typedef struct {
    int32_t sev[GATE_QM_N_CHANNELS];   /* 0 ok / 1 warn (gray) / 2 bad   */
    uint8_t gray;                      /* bitmask: gray-zone channels    */
    uint8_t dissent;                   /* bitmask: near-edge channels    */
    int32_t penalty_u;                 /* summed penalty, micro-units    */
    int32_t verdict;                   /* GATE_VERDICT_*                 */
    int32_t worst;                     /* worst severity over channels   */
} gate_judgment_t;

/* Judge one bar: six channel readings in µ, channel order = the frozen
 * CRITIC_FEATURES order carried by critic-gate.qm. Integer-only, O(6),
 * no allocation, no globals — pure function. */
void critic_gate_judge(const int32_t features_u[GATE_QM_N_CHANNELS],
                       gate_judgment_t *out);

/* Judge a single channel reading (ledger anchor probes): severity + gray
 * + dissent for that channel alone. */
void critic_gate_judge_channel(int ch_idx, int32_t value_u,
                               int32_t *sev, int *gray, int *dissent);

/* The mint receipt: sha256 of the exact gate-bands.json bytes the .qm
 * was minted from (defined in gate_qm.h). Printed at boot — provenance
 * on metal. */
const char *critic_gate_sha256(void);

#endif /* CRITIC_GATE_H */
