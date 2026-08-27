/* eileen_limb.c — THE EILEEN's metal spine: the stem's sentences → the
 * ten pieces → the figurehead's closing, all through the canon verbs
 * (qm_bind/qm_link/qm_effect/qm_view/qm_tick). Host harness and ESP32
 * firmware compile THIS file unchanged. Integer day counts only; the
 * chain and the joint order are compile-time frozen from eileen_qm.h
 * (mint-receipt sealed). */
#include "eileen_limb.h"

#include <stdlib.h>
#include <string.h>

const char *const eileen_cell_names[EILEEN_QM_N_CELLS] = {
    "keel", "stem", "keelson", "breast_hook", "rigging",
    "bulwarks", "ensign", "scuppers", "sheerboard", "figurehead",
};

/* stringify the integer version define into the facts canon */
#define QM_STR_(x) #x
#define QM_STR(x) QM_STR_(x)

/* ── BIND: the sheet ── */

int eileen_limb_init(qvm_t **vm_out, eileen_cells_t *cells)
{
    if (!vm_out || !cells) return -1;
    qvm_t *vm = qvm_new();
    if (!vm) return -1;

    int32_t *cell_ptrs[EILEEN_QM_N_CELLS] = {
        &cells->keel, &cells->stem, &cells->keelson, &cells->breast_hook,
        &cells->rigging, &cells->bulwarks, &cells->ensign, &cells->scuppers,
        &cells->sheerboard, &cells->figurehead,
    };
    for (int i = 0; i < EILEEN_QM_N_CELLS; i++) {
        /* value points into the caller's cells — primitive, no free */
        if (qm_bind(vm, eileen_cell_names[i], cell_ptrs[i], NULL) != 0) {
            qvm_free(vm);
            return -1;
        }
    }

    /* limb + spine + the closing line, canon-string facts for boot print */
    if (qm_bind_str(vm, "eileen-root",
            "{\"name\":\"eileen-root\",\"role\":\"minimal spine — "
            "escalation target of record\",\"rule_count\":0}") != 0
     || qm_bind_str(vm, "eileen-limb:facts",
            "{\"organism\":\"eileen-limb\",\"rules\":\"eileen.qm\","
            "\"rule_version\":" QM_STR(EILEEN_QM_VERSION) ","
            "\"sha256\":\"" EILEEN_QM_SHA256 "\","
            "\"pieces\":10,"
            "\"joint_order\":\"keel stem keelson breast_hook rigging "
            "bulwarks ensign scuppers sheerboard figurehead\","
            "\"fixed_point\":\"integer day counts, no floats\","
            "\"keel\":\"value 1 — the 13:19 blink, ♩=60\"}") != 0
     || qm_bind(vm, "eileen-limb:response", NULL, NULL) != 0
     || qm_bind(vm, "eileen-limb:day", NULL, NULL) != 0
     || qm_bind_str(vm, "log.figurehead",
            "\"" EILEEN_QM_LOG "\"") != 0
     || qm_link(vm, "eileen-limb", "eileen-root", "lineage") != 0) {
        qvm_free(vm);
        return -1;
    }

    /* the keel is laid first: value 1 before any water arrives */
    cells->keel = EILEEN_KEEL_VALUE;
    cells->stem = 0;
    cells->keelson = 0;
    cells->breast_hook = 0;
    cells->rigging = 0;
    cells->bulwarks = 0;
    cells->ensign = 0;
    cells->scuppers = 0;
    cells->sheerboard = 0;
    cells->figurehead = 0;
    *vm_out = vm;
    return 0;
}

/* ── absorb: a completed sentence advances the day ── */

void eileen_limb_absorb(eileen_cells_t *cells, const nmea_sentence_t *s)
{
    (void)s;   /* any sentence that survived the stem's tally counts:
                  known or unknown, the water was true (checksum held) */
    if (cells->stem < EILEEN_QM_DAY_MAX)
        cells->stem++;
    /* saturation at EILEEN_QM_DAY_MAX is the integer-discipline wall:
       sheerboard = 5N+3 stays inside int32; the days stop counting
       before the wood can overflow */
}

/* ── the figurehead's listener ── */

int eileen_figurehead_fires(int32_t sheerboard)
{
    return sheerboard > 0 ? 1 : 0;
}

/* ── the dependency chain (integer-only, pure, joint order) ── */

void eileen_chain(eileen_cells_t *cells, eileen_day_t *out)
{
    int n = 0;

    /* the joint order IS the frozen order: keel laid, stem takes the
     * water, then each piece reads the one before it, in order */
    out->order[n++] = EILEEN_CELL_KEEL;         /* laid first, constant  */
    cells->keel = EILEEN_KEEL_VALUE;

    out->order[n++] = EILEEN_CELL_STEM;         /* the day counter       */

    out->order[n++] = EILEEN_CELL_KEELSON;      /* =stem + keel          */
    cells->keelson = cells->stem + cells->keel;

    out->order[n++] = EILEEN_CELL_BREAST_HOOK;  /* =keelson * 2          */
    cells->breast_hook = cells->keelson * 2;

    out->order[n++] = EILEEN_CELL_RIGGING;      /* =breast_hook + keelson */
    cells->rigging = cells->breast_hook + cells->keelson;

    out->order[n++] = EILEEN_CELL_BULWARKS;     /* =rigging - 2          */
    cells->bulwarks = cells->rigging - 2;

    out->order[n++] = EILEEN_CELL_ENSIGN;       /* =bulwarks + 1         */
    cells->ensign = cells->bulwarks + 1;

    out->order[n++] = EILEEN_CELL_SCUPPERS;     /* =ensign - 1           */
    cells->scuppers = cells->ensign - 1;

    out->order[n++] = EILEEN_CELL_SHEERBOARD;   /* =scuppers+breast_hook */
    cells->sheerboard = cells->scuppers + cells->breast_hook;

    out->order[n++] = EILEEN_CELL_FIGUREHEAD;   /* closes on the keel    */
    out->fired = eileen_figurehead_fires(cells->sheerboard);
    cells->figurehead = out->fired ? cells->keel : 0;

    out->v = *cells;
    out->day = cells->stem;
}

/* ── the keel's beat: ♩=60 ── */

int eileen_keel_phase(unsigned long ms)
{
    /* half a second lit, half a second dark — the 13:19 blink's table,
     * the same tempo movement I of the bread score plays */
    return (ms / EILEEN_KEEL_BLINK_MS) % 2 == 0 ? 1 : 0;
}

/* ── EFFECT + TICK + VIEW: the serve ── */

/* fwd: install the day copy (arg) into the day thing — ownership passes
 * to the thing; its destructor frees it on the next set (the
 * heap-own-the-arg lesson from qm_opcodes: no stack shell to outlive) */
static void fwd_day(qvm_thing_t *t, void *arg)
{
    qvm_thing_set(t, arg, free);
}
static void inv_day(qvm_thing_t *t, void *arg)
{
    (void)arg;
    qvm_thing_set(t, NULL, NULL);
}

int eileen_limb_serve(qvm_t *vm, eileen_cells_t *cells,
                      eileen_day_t *out, char resp_out[EILEEN_RESP_MAX])
{
    eileen_day_t day;
    eileen_chain(cells, &day);

    eileen_day_t *copy = malloc(sizeof *copy);
    if (!copy) return -1;
    *copy = day;
    if (qm_effect(vm, "eileen-limb:day", fwd_day, inv_day, copy) != 0) {
        free(copy);
        return -1;
    }

    /* canon response: sorted keys (canonical JSON), integers only */
    char resp[EILEEN_RESP_MAX];
    resp[0] = '\0';
    snprintf(resp, sizeof resp,
             "{\"breast_hook\":%ld,\"bulwarks\":%ld,\"day\":%ld,"
             "\"ensign\":%ld,\"figurehead\":%ld,\"fired\":%d,"
             "\"keel\":%ld,\"keelson\":%ld,\"rigging\":%ld,"
             "\"scuppers\":%ld,\"sheerboard\":%ld,\"stem\":%ld}",
             (long)day.v.breast_hook, (long)day.v.bulwarks, (long)day.day,
             (long)day.v.ensign, (long)day.v.figurehead, day.fired,
             (long)day.v.keel, (long)day.v.keelson, (long)day.v.rigging,
             (long)day.v.scuppers, (long)day.v.sheerboard,
             (long)day.v.stem);
    if (qm_effect_set(vm, "eileen-limb:response", resp) != 0)
        return -1;

    qm_tick(vm, 1.0);   /* drains: day + response land */

    if (out) *out = day;
    if (resp_out) {
        strncpy(resp_out, resp, EILEEN_RESP_MAX - 1);
        resp_out[EILEEN_RESP_MAX - 1] = '\0';
    }
    return 0;
}

void eileen_limb_free(qvm_t *vm)
{
    qvm_free(vm);
}
