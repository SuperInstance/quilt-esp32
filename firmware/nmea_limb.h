/* nmea_limb.h — the vessel limb wiring: parsed NMEA sentences → quilt
 * cells → band rules → alert, through the five canon opcodes (Paper 211).
 *
 * The division of labor (documented, mirrors blink.qm/reflex patterns):
 *
 *   BIND    the sheet: seven cells from vessel-demo.qm's frozen order —
 *           ais.position.lat/lon, own.fix, own.heading, own.depth,
 *           own.sog, own.wspeed — each thing's value is a pointer INTO
 *           the caller's vessel_cells_t (no copy, no free; the sheet IS
 *           the cells). Cell values update in place as sentences absorb:
 *           the sheet always reflects the latest instrument reading.
 *   LINK    vessel-limb → vessel-root lineage (like blink/reflex).
 *   EFFECT  the rule evaluation: each serve queues a NEW judgment into
 *           "vessel-limb:alert" (heap copy — the arg must outlive the
 *           tick, the qm_opcodes lesson) and a canon-JSON response into
 *           "vessel-limb:response".
 *   TICK    drains the pending effects — the alert/response land.
 *   VIEW    projects cells (int32 µ-units), the alert judgment, and the
 *           canon response for whoever reads (host demo, firmware log,
 *           the helm box over UART).
 *
 * Rules come from vessel-demo.qm compiled by qm_vessel2c.py
 * (vessel_qm.h) — mint-receipt sha256 at boot, micro-units, no floats.
 * The limb never parses JSON on target; it only emits canon.
 *
 * C99 + stdlib (strdup ok on newlib); no POSIX, no stdio in the limb
 * path (the response buffer is written with snprintf, which qm_serve
 * already uses on target — Arduino has it).
 */
#ifndef NMEA_LIMB_H
#define NMEA_LIMB_H

#include "quilt_vm.h"
#include "qm_opcodes.h"
#include "nmea.h"
#include "vessel_qm.h"

/* severity codes — same shape as critic-gate, one alert tier per rule */
#define VESSEL_SEV_NA  (-1)  /* rule not judged (no data yet) */
#define VESSEL_SEV_OK  0
#define VESSEL_SEV_WARN 1
#define VESSEL_SEV_BAD 2

/* the sheet cells — vessel-demo.qm's frozen order; a thing's value
 * points at the matching member (VIEW returns int32_t*) */
typedef struct {
    int32_t lat_udeg;      /* ais.position.lat  (mirrors own until AIS)  */
    int32_t lon_udeg;      /* ais.position.lon                            */
    int32_t fix;           /* own.fix: 0/1                                */
    int32_t heading_udeg;  /* own.heading                                 */
    int32_t depth_um;      /* own.depth  (0 = no reading yet)             */
    int32_t sog_ukn;       /* own.sog                                     */
    int32_t wspeed_ukn;    /* own.wspeed                                  */
} vessel_cells_t;

/* one rule-table evaluation (all fields VESSEL_SEV_*) */
typedef struct {
    int geofence;
    int depth;
    int drift;
    int nofix;
    int alert;             /* worst judged severity: 0 clear/1 watch/2 alert */
} vessel_judgment_t;

/* cell thing names in frozen order (thing = name, value = &cell) */
extern const char *const vessel_cell_names[VESSEL_QM_N_CELLS];

/* qvm_new + BIND cells + BIND limb/root/alert/response + LINK lineage.
 * cells must outlive the vm (thing values point into it). Returns 0. */
int vessel_limb_init(qvm_t **vm_out, vessel_cells_t *cells);

/* fold one parsed sentence into the cells (known sentences only —
 * caller checks nmea_sentence_t.known). GGA/RMC set position+fix (and
 * sog/cog for RMC), HDT heading, DBT depth, VHW water speed. ais.position
 * mirrors own position until the AIVDM lane exists. */
void vessel_limb_absorb(vessel_cells_t *cells, const nmea_sentence_t *s);

/* the rule table, integer-only, pure function: vessel_qm.h constants
 * against the cells. geofence judges only with a fix; depth skips when
 * no reading (0); alert = worst judged severity. */
void vessel_judge(const vessel_cells_t *cells, vessel_judgment_t *out);

/* one serve: judge → EFFECT the judgment into "vessel-limb:alert" and a
 * canon-JSON summary into "vessel-limb:response" → TICK(1.0) → copy the
 * judgment out. resp_out (may be NULL) receives the canon response
 * (buffer >= VESSEL_RESP_MAX, always NUL-terminated; integers only).
 * Returns 0 ok, -1 VM/alloc error. */
#define VESSEL_RESP_MAX 96
int vessel_limb_serve(qvm_t *vm, vessel_cells_t *cells,
                      vessel_judgment_t *out, char resp_out[VESSEL_RESP_MAX]);

/* qvm_free (the documented upstream leak applies: last alert copy and
 * effect-record target strs — same class blink documents). */
void vessel_limb_free(qvm_t *vm);

#endif /* NMEA_LIMB_H */
