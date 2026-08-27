/* eileen_limb.h — THE EILEEN on metal: the stem's sentences → the ten
 * named pieces → the figurehead's closing, through the five canon
 * opcodes (Paper 211). One dependency chain, evaluated in the manifest's
 * joint order every time a completed sentence advances the day.
 *
 * The division of labor (mirrors nmea_limb / blink / reflex):
 *
 *   BIND    the sheet: ten cells from eileen.qm's frozen joint order —
 *           keel, stem, keelson, breast_hook, rigging, bulwarks, ensign,
 *           scuppers, sheerboard, figurehead — each thing's value is a
 *           pointer INTO the caller's eileen_cells_t (the sheet IS the
 *           cells), plus log.figurehead (the closing line, canon string).
 *   LINK    eileen-limb → eileen-root lineage (like blink/reflex/nmea).
 *   EFFECT  each serve queues the day's evaluation (heap copy — the arg
 *           must outlive the tick, the qm_opcodes lesson) into
 *           "eileen-limb:day" and a canon-JSON response into
 *           "eileen-limb:response".
 *   TICK    drains the pending effects — the day + response land.
 *   VIEW    projects the ten cells (int32 day counts), the day record,
 *           and the canon response for whoever reads.
 *
 * The chain comes from eileen.qm compiled by qm_eileen2c.py
 * (eileen_qm.h) — mint-receipt sha256 at boot, integer day counts, no
 * floats. The keel drives the LED at ♩=60 (EILEEN_KEEL_BLINK_MS): it is
 * the same half-second-on / half-second-off rule table as blink.qm, the
 * 2026-08-26 13:19 blink — movement I of the bread score.
 *
 * C99 + stdlib (strdup ok on newlib); no POSIX, no stdio in the limb
 * path (the response buffer is written with snprintf — Arduino has it).
 */
#ifndef EILEEN_LIMB_H
#define EILEEN_LIMB_H

#include "quilt_vm.h"
#include "qm_opcodes.h"
#include "nmea.h"
#include "eileen_qm.h"

/* the sheet cells — eileen.qm's frozen joint order; a thing's value
 * points at the matching member (VIEW returns int32_t*) */
typedef struct {
    int32_t keel;         /* keel:        value 1 — the blink           */
    int32_t stem;         /* stem:        sensor — completed sentences  */
    int32_t keelson;      /* keelson:     stem + keel                  */
    int32_t breast_hook;  /* breast_hook: keelson * 2                  */
    int32_t rigging;      /* rigging:     breast_hook + keelson        */
    int32_t bulwarks;     /* bulwarks:    rigging - 2                  */
    int32_t ensign;       /* ensign:      bulwarks + 1                 */
    int32_t scuppers;     /* scuppers:    ensign - 1                   */
    int32_t sheerboard;   /* sheerboard:  scuppers + breast_hook       */
    int32_t figurehead;   /* figurehead:  closes on keel when sheered  */
} eileen_cells_t;

/* one day's evaluation: the values, the day number, whether the
 * figurehead fired, and the order the pieces were evaluated in */
typedef struct {
    eileen_cells_t v;
    int32_t day;                    /* == v.stem                          */
    int fired;                      /* figurehead listener fired          */
    uint8_t order[EILEEN_QM_N_CELLS]; /* evaluation order, joint by joint */
} eileen_day_t;

/* cell thing names in frozen joint order (thing = name, value = &cell) */
extern const char *const eileen_cell_names[EILEEN_QM_N_CELLS];

/* qvm_new + BIND cells + BIND limb/root/log + LINK lineage; keel = 1,
 * all else 0. cells must outlive the vm (thing values point into it).
 * Returns 0 ok, -1 error. */
int eileen_limb_init(qvm_t **vm_out, eileen_cells_t *cells);

/* a completed sentence advances the day: stem += 1 (the stem's quiet
 * tally — only checksum-valid water gets through; the caller feeds
 * NMEA_DONE sentences only, known or unknown both count: what survives
 * the tally advances the day). Saturates at EILEEN_QM_DAY_MAX. */
void eileen_limb_absorb(eileen_cells_t *cells, const nmea_sentence_t *s);

/* the figurehead's listener condition, pure — the only branch in the
 * chain. Fires iff sheerboard > 0 (the steel sheet's condition). */
int eileen_figurehead_fires(int32_t sheerboard);

/* the dependency chain, integer-only, pure: evaluates the pieces in the
 * manifest's joint order (order recorded in out->order, must equal
 * EILEEN_QM_JOINTS), each piece reading only its predecessors, exactly
 * as the joints describe. day = stem; figurehead = keel when the
 * listener fires. */
void eileen_chain(eileen_cells_t *cells, eileen_day_t *out);

/* the keel's beat, pure: ♩=60 — phase 1 (lit) for the first 500 ms of
 * each second, 0 (dark) for the second 500 ms. ms is millis()-class. */
int eileen_keel_phase(unsigned long ms);

/* one serve: chain → EFFECT the day record into "eileen-limb:day" and a
 * canon-JSON summary into "eileen-limb:response" → TICK(1.0) → copy the
 * day out. resp_out (may be NULL) receives the canon response (buffer
 * >= EILEEN_RESP_MAX, always NUL-terminated; integers only, sorted
 * keys). Returns 0 ok, -1 VM/alloc error. */
#define EILEEN_RESP_MAX 288
int eileen_limb_serve(qvm_t *vm, eileen_cells_t *cells,
                      eileen_day_t *out, char resp_out[EILEEN_RESP_MAX]);

/* qvm_free (the documented upstream leak applies: last day copy and
 * effect-record target strs — same class blink/nmea document). */
void eileen_limb_free(qvm_t *vm);

#endif /* EILEEN_LIMB_H */
