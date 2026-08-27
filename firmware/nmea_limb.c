/* nmea_limb.c — parsed NMEA → quilt cells → vessel-demo.qm rules →
 * alert, all through the canon verbs (qm_bind/qm_link/qm_effect/
 * qm_view/qm_tick). Host harness and ESP32 firmware compile THIS file
 * unchanged. Integer micro-units only; the rule thresholds are compile-
 * time constants from vessel_qm.h (mint-receipt sealed). */
#include "nmea_limb.h"

#include <stdlib.h>
#include <string.h>

const char *const vessel_cell_names[VESSEL_QM_N_CELLS] = {
    "ais.position.lat", "ais.position.lon", "own.fix",
    "own.heading", "own.depth", "own.sog", "own.wspeed",
};

/* stringify the integer version define into the facts canon */
#define QM_STR_(x) #x
#define QM_STR(x) QM_STR_(x)

/* ── BIND: the sheet ── */

int vessel_limb_init(qvm_t **vm_out, vessel_cells_t *cells)
{
    if (!vm_out || !cells) return -1;
    qvm_t *vm = qvm_new();
    if (!vm) return -1;

    int32_t *cell_ptrs[VESSEL_QM_N_CELLS] = {
        &cells->lat_udeg, &cells->lon_udeg, &cells->fix,
        &cells->heading_udeg, &cells->depth_um, &cells->sog_ukn,
        &cells->wspeed_ukn,
    };
    for (int i = 0; i < VESSEL_QM_N_CELLS; i++) {
        /* value points into the caller's cells — primitive, no free */
        if (qm_bind(vm, vessel_cell_names[i], cell_ptrs[i], NULL) != 0) {
            qvm_free(vm);
            return -1;
        }
    }

    /* limb + spine + output things, canon-string facts for boot print */
    if (qm_bind_str(vm, "vessel-root",
            "{\"name\":\"vessel-root\",\"role\":\"minimal spine — "
            "escalation target of record\",\"rule_count\":0}") != 0
     || qm_bind_str(vm, "vessel-limb:facts",
            "{\"organism\":\"vessel-limb\",\"rules\":\"vessel-demo.qm\","
            "\"rule_version\":" QM_STR(VESSEL_QM_VERSION) ","
            "\"sha256\":\"" VESSEL_QM_SHA256 "\","
            "\"fixed_point\":\"micro-units 1e-6, integer-only\","
            "\"rule_count\":4}") != 0
     || qm_bind(vm, "vessel-limb:response", NULL, NULL) != 0
     || qm_bind(vm, "vessel-limb:alert", NULL, NULL) != 0
     || qm_link(vm, "vessel-limb", "vessel-root", "lineage") != 0) {
        qvm_free(vm);
        return -1;
    }
    *vm_out = vm;
    return 0;
}

/* ── absorb: sentence fields → cell values ── */

void vessel_limb_absorb(vessel_cells_t *cells, const nmea_sentence_t *s)
{
    if (!s->known) return;
    const char *type = s->addr + 2;
    if (type[0] == 'G' && type[1] == 'G' && type[2] == 'A') {
        cells->fix = s->has_fix;
        if (s->has_fix) {
            cells->lat_udeg = s->lat_udeg;   /* ais.position mirrors own */
            cells->lon_udeg = s->lon_udeg;   /* until the AIVDM lane     */
        }
    } else if (type[0] == 'R' && type[1] == 'M' && type[2] == 'C') {
        cells->fix = s->has_fix;
        if (s->has_fix) {
            cells->lat_udeg = s->lat_udeg;
            cells->lon_udeg = s->lon_udeg;
            cells->sog_ukn = s->sog_ukn;
        }
    } else if (type[0] == 'H' && type[1] == 'D' && type[2] == 'T') {
        cells->heading_udeg = s->heading_udeg;
    } else if (type[0] == 'D' && type[1] == 'B' && type[2] == 'T') {
        cells->depth_um = s->depth_um;
    } else if (type[0] == 'V' && type[1] == 'H' && type[2] == 'W') {
        cells->wspeed_ukn = s->wspeed_ukn;
    }
}

/* ── the rule table (integer-only, pure) ── */

/* one box axis: ok inside inclusive; warn outside within margin or
 * inside within margin of a boundary; bad outside beyond margin */
static int geofence_axis(int32_t v, int32_t lo, int32_t hi, int32_t margin)
{
    if (v < lo || v > hi) {
        int32_t out = v < lo ? lo - v : v - hi;
        return out > margin ? VESSEL_SEV_BAD : VESSEL_SEV_WARN;
    }
    int32_t edge = (v - lo) < (hi - v) ? (v - lo) : (hi - v);
    return edge <= margin ? VESSEL_SEV_WARN : VESSEL_SEV_OK;
}

static int worst(int a, int b) { return a > b ? a : b; }

void vessel_judge(const vessel_cells_t *cells, vessel_judgment_t *out)
{
    /* geofence: only meaningful with a fix (0,0 is "no position", not
     * "off Somalia") */
    if (cells->fix) {
        int s_lat = geofence_axis(cells->lat_udeg, VESSEL_QM_GEOFENCE.lat_lo,
                                  VESSEL_QM_GEOFENCE.lat_hi,
                                  VESSEL_QM_GEOFENCE.margin);
        int s_lon = geofence_axis(cells->lon_udeg, VESSEL_QM_GEOFENCE.lon_lo,
                                  VESSEL_QM_GEOFENCE.lon_hi,
                                  VESSEL_QM_GEOFENCE.margin);
        out->geofence = worst(s_lat, s_lon);
    } else {
        out->geofence = VESSEL_SEV_NA;
    }

    /* depth: 0 = no reading yet (a true 0.0 m reading means the keel is
     * in the mud — the instrument sends empty instead) */
    if (cells->depth_um == 0) {
        out->depth = VESSEL_SEV_NA;
    } else if (cells->depth_um < VESSEL_QM_DEPTH_ALERT_BELOW) {
        out->depth = VESSEL_SEV_BAD;
    } else if (cells->depth_um < VESSEL_QM_DEPTH_WARN_BELOW) {
        out->depth = VESSEL_SEV_WARN;
    } else {
        out->depth = VESSEL_SEV_OK;
    }

    /* drift: anchored context — sog above the bands means the anchor
     * isn't holding (or nobody told the limb it's underway) */
    if (cells->sog_ukn > VESSEL_QM_DRIFT_ALERT_ABOVE) {
        out->drift = VESSEL_SEV_BAD;
    } else if (cells->sog_ukn > VESSEL_QM_DRIFT_WARN_ABOVE) {
        out->drift = VESSEL_SEV_WARN;
    } else {
        out->drift = VESSEL_SEV_OK;
    }

    /* nofix: the .qm says warn when the fix drops */
    out->nofix = cells->fix ? VESSEL_SEV_OK : VESSEL_QM_NOFIX_SEV;

    out->alert = worst(worst(out->geofence, out->depth),
                       worst(out->drift, out->nofix));
    if (out->alert < VESSEL_SEV_OK) out->alert = VESSEL_SEV_OK; /* NA floor */
}

/* ── EFFECT + TICK + VIEW: the serve ── */

/* fwd: install the judgment copy (arg) into the alert thing — ownership
 * passes to the thing; its destructor frees it on the next set (the
 * heap-own-the-arg lesson from qm_opcodes: no stack shell to outlive) */
static void fwd_alert(qvm_thing_t *t, void *arg)
{
    qvm_thing_set(t, arg, free);
}
static void inv_alert(qvm_thing_t *t, void *arg)
{
    (void)arg;
    qvm_thing_set(t, NULL, NULL);
}

/* stringify helper — VESSEL_SEV_* as one digit for canon */
#define SEV_CH(s) ((s) < 0 ? '-' : ('0' + (s)))

int vessel_limb_serve(qvm_t *vm, vessel_cells_t *cells,
                      vessel_judgment_t *out, char resp_out[VESSEL_RESP_MAX])
{
    vessel_judgment_t j;
    vessel_judge(cells, &j);

    vessel_judgment_t *copy = malloc(sizeof *copy);
    if (!copy) return -1;
    *copy = j;
    if (qm_effect(vm, "vessel-limb:alert", fwd_alert, inv_alert, copy) != 0) {
        free(copy);
        return -1;
    }

    /* canon response: sorted keys (canonical JSON), integers only */
    char resp[VESSEL_RESP_MAX];
    resp[0] = '\0';
    snprintf(resp, sizeof resp,
             "{\"alert\":%d,\"depth\":%d,\"drift\":%d,\"geofence\":%d,"
             "\"nofix\":%d}",
             j.alert, j.depth, j.drift, j.geofence, j.nofix);
    if (qm_effect_set(vm, "vessel-limb:response", resp) != 0)
        return -1;

    qm_tick(vm, 1.0);   /* drains: alert + response land */

    if (out) *out = j;
    if (resp_out) {
        strncpy(resp_out, resp, VESSEL_RESP_MAX - 1);
        resp_out[VESSEL_RESP_MAX - 1] = '\0';
    }
    return 0;
}

void vessel_limb_free(qvm_t *vm)
{
    qvm_free(vm);
}
