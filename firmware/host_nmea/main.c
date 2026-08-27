/* host_nmea/main.c — the NMEA lane on the desktop: unit tests for the
 * parser (per sentence type + checksum edges), the rule table, and the
 * canon-verb wiring; then the vessel demo — a fixture stream of
 * real-format NMEA sentences replayed through the parser into the sheet,
 * alerts verified fire/not-fire per the rule table. One JSON line per
 * sentence (the demo transcript), one summary line, exit 0 iff all green.
 *
 *   ./host_nmea_bin [vessel-stream.txt]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmea.h"
#include "nmea_limb.h"

static int passed = 0, failed = 0;
#define CKOK(s) nmea_checksum_ok((s), strlen(s))
#define CHECK(cond, name) do { \
    if (cond) { passed++; } \
    else { failed++; printf("{\"fail\":\"%s\"}\n", name); } \
} while (0)

/* feed a full line (adds '\n'); returns the interesting code: if any
 * byte returned NMEA_OVERFLOW that wins (the '\n' itself returns MORE),
 * else the final byte's code */
static int feed_str(nmea_ctx_t *ctx, const char *s)
{
    int rc = NMEA_MORE, saw = NMEA_MORE;
    for (const char *p = s; *p; p++) {
        rc = nmea_feed(ctx, *p);
        if (rc != NMEA_MORE && saw == NMEA_MORE) saw = rc;
    }
    rc = nmea_feed(ctx, '\n');
    return rc != NMEA_MORE ? rc : saw;
}

/* ── unit tests ────────────────────────────────────────────────────── */

static void test_checksum(void)
{
    /* the classic spec examples */
    CHECK(CKOK("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47") == 0,
          "checksum: spec GGA example");
    CHECK(CKOK("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A") == 0,
          "checksum: spec RMC example");
    CHECK(CKOK("$HCHDT,275.4,T*2D") == 0,
          "checksum: HDT");
    CHECK(CKOK("$HCHDT,275.4,T*2d") == 0,
          "checksum: lowercase hex accepted");
    CHECK(CKOK("$HCHDT,275.4,T*2E") != 0,
          "checksum: wrong value rejected");
    CHECK(CKOK("$HCHDT,275.4,T") != 0,
          "checksum: missing *HH rejected");
    CHECK(CKOK("$HCHDT,275.4,T*2D7") != 0,
          "checksum: trailing junk rejected");
    CHECK(CKOK("HCHDT,275.4,T*2D") != 0,
          "checksum: no $ rejected");
}

static void test_fixed_to_u(void)
{
    int32_t u;
    CHECK(nmea_fixed_to_u("022.4", 5, &u) == 0 && u == 22400000, "fixed: 022.4");
    CHECK(nmea_fixed_to_u("18.0", 4, &u) == 0 && u == 18000000, "fixed: 18.0");
    CHECK(nmea_fixed_to_u("5", 1, &u) == 0 && u == 5000000, "fixed: integer 5");
    CHECK(nmea_fixed_to_u(".5", 2, &u) != 0, "fixed: .5 rejected (integer part required)");
    CHECK(nmea_fixed_to_u("12.3456789", 10, &u) == 0 && u == 12345678,
          "fixed: 7th fraction digit truncates to µ grid");
    CHECK(nmea_fixed_to_u("2001.0", 6, &u) != 0, "fixed: cap 2000 rejected");
    CHECK(nmea_fixed_to_u("", 0, &u) != 0, "fixed: empty rejected");
    CHECK(nmea_fixed_to_u("1e3", 3, &u) != 0, "fixed: non-digit rejected");
    CHECK(nmea_fixed_to_u("-3.5", 4, &u) != 0, "fixed: sign rejected (hemi is separate)");
}

static void test_coords(void)
{
    int32_t d;
    CHECK(nmea_coord_to_udeg("4807.038", 8, 'N', 0, &d) == 0 && d == 48117300,
          "coord: 4807.038 N -> +48117300");
    CHECK(nmea_coord_to_udeg("4807.038", 8, 'S', 0, &d) == 0 && d == -48117300,
          "coord: S negates");
    CHECK(nmea_coord_to_udeg("01131.000", 9, 'E', 1, &d) == 0 && d == 11516666,
          "coord: 01131.000 E -> +11516666");
    CHECK(nmea_coord_to_udeg("15224.500", 9, 'W', 1, &d) == 0 && d == -152408333,
          "coord: Kodiak 15224.500 W -> -152408333");
    CHECK(nmea_coord_to_udeg("5747.900", 8, 'N', 0, &d) == 0 && d == 57798333,
          "coord: Kodiak 5747.900 N -> 57798333");
    CHECK(nmea_coord_to_udeg("5745.003", 8, 'N', 0, &d) == 0 && d == 57750050,
          "coord: boundary hugging 5745.003 N");
    CHECK(nmea_coord_to_udeg("4860.000", 8, 'N', 0, &d) != 0, "coord: mm=60 rejected");
    CHECK(nmea_coord_to_udeg("9130.000", 8, 'N', 0, &d) != 0, "coord: lat >90 rejected");
    CHECK(nmea_coord_to_udeg("18100.000", 9, 'E', 1, &d) != 0, "coord: lon >180 rejected");
    CHECK(nmea_coord_to_udeg("4807.038", 8, 'N', 1, &d) != 0, "coord: N with is_lon rejected");
    CHECK(nmea_coord_to_udeg("4807.038", 8, 'X', 0, &d) != 0, "coord: bad hemisphere rejected");
    CHECK(nmea_coord_to_udeg("7.038", 5, 'N', 0, &d) != 0, "coord: too few integer digits rejected");
}

static void test_sentences(void)
{
    nmea_ctx_t ctx;
    nmea_reset(&ctx);

    CHECK(feed_str(&ctx, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47") == NMEA_DONE,
          "GGA: DONE");
    CHECK(ctx.out.known && strcmp(ctx.out.addr, "GPGGA") == 0 && ctx.out.has_fix
          && ctx.out.fix_quality == 1 && ctx.out.n_sat == 8
          && ctx.out.lat_udeg == 48117300 && ctx.out.lon_udeg == 11516666,
          "GGA: fix, sats, position");

    CHECK(feed_str(&ctx, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A") == NMEA_DONE,
          "RMC: DONE");
    CHECK(ctx.out.known && strcmp(ctx.out.addr, "GPRMC") == 0 && ctx.out.has_fix
          && ctx.out.sog_ukn == 22400000 && ctx.out.cog_udeg == 84400000,
          "RMC: sog + cog");

    CHECK(feed_str(&ctx, "$GPRMC,120030,V,5747.900,N,15224.500,W,1.2,090.0,260826,,,A*7A") == NMEA_DONE,
          "RMC: status V DONE");
    CHECK(!ctx.out.has_fix, "RMC: status V -> no fix");

    CHECK(feed_str(&ctx, "$HCHDT,275.4,T*2D") == NMEA_DONE
          && ctx.out.known && ctx.out.heading_udeg == 275400000,
          "HDT: heading 275.4 deg");

    CHECK(feed_str(&ctx, "$SDDBT,65.6,f,20.0,M,10.9,F*39") == NMEA_DONE
          && ctx.out.known && ctx.out.depth_um == 20000000,
          "DBT: meters field");
    CHECK(feed_str(&ctx, "$SDDBT,65.6,f,,M,10.9,F*25") == NMEA_DONE
          && ctx.out.depth_um == 19994880,
          "DBT: empty meters -> feet*0.3048");
    CHECK(feed_str(&ctx, "$SDDBT,,,,,10.9,F*15") == NMEA_DONE
          && ctx.out.depth_um == 19933920,
          "DBT: fathoms fallback (10.9 fm × 1.8288 m)");
    CHECK(feed_str(&ctx, "$SDDBT,65.6,f,,M,10.9,F*00") == NMEA_BAD_CK,
          "DBT: corrupt checksum -> BAD_CK");

    CHECK(feed_str(&ctx, "$VWVHW,275.4,T,,M,1.5,N,2.8,K*70") == NMEA_DONE
          && ctx.out.known && ctx.out.wspeed_ukn == 1500000,
          "VHW: knots field");
    CHECK(feed_str(&ctx, "$VWVHW,,T,,M,,N,6.9,K*75") == NMEA_DONE
          && ctx.out.wspeed_ukn == 1916666,
          "VHW: empty knots -> km/h*5/18");

    CHECK(feed_str(&ctx, "$GPZDA,160012.71,11,03,2004,-1,00*7D") == NMEA_DONE
          && !ctx.out.known && strcmp(ctx.out.addr, "GPZDA") == 0,
          "ZDA: valid but unknown -> known=0");

    CHECK(feed_str(&ctx, "$HCHDT,275.4,M*34") == NMEA_BAD_FMT,
          "HDT: sensor 'M' where 'T' belongs -> BAD_FMT");
    CHECK(feed_str(&ctx, "$GPGGA,123519,48XX.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*40") == NMEA_BAD_FMT,
          "GGA: garbage lat with valid checksum -> BAD_FMT");
    CHECK(feed_str(&ctx, "$HCHDT,275.4,T") == NMEA_BAD_CK,
          "no *HH -> BAD_CK (checksum required)");
    CHECK(feed_str(&ctx, "$HCHDT,275.4,T*2D7") == NMEA_BAD_CK,
          "junk after HH -> BAD_CK");

    /* CRLF + noise + resync */
    {
        int rc = NMEA_MORE;
        const char *noise = "x y z\r\n$HCHDT,3$HCHDT,180.0,T*";
        for (const char *p = noise; *p; p++) rc = nmea_feed(&ctx, *p);
        rc = nmea_feed(&ctx, '2'); rc = nmea_feed(&ctx, '0'); rc = nmea_feed(&ctx, '\n');
        CHECK(rc == NMEA_DONE && ctx.out.heading_udeg == 180000000,
              "framing: noise, CR, mid-line $ resync");
    }

    /* overflow: one long line, one return, then resync */
    {
        char big[160];
        memset(big, 'A', sizeof big - 1); big[0] = '$';
        memcpy(big + 1, "GPTXT,", 6);
        int rc = NMEA_MORE;
        for (size_t i = 0; i < sizeof big - 1; i++) {
            rc = nmea_feed(&ctx, big[i]);
            if (rc == NMEA_OVERFLOW) break;
        }
        CHECK(rc == NMEA_OVERFLOW, "overflow: NMEA_OVERFLOW when the buffer fills");
        rc = feed_str(&ctx, "");   /* terminate the swallowed line */
        (void)rc;
        CHECK(feed_str(&ctx, "$HCHDT,090.0,T*20") == NMEA_DONE,
              "overflow: parser resyncs after drop");
    }

    CHECK(ctx.n_overflow == 1, "counters: one overflow counted");
    CHECK(ctx.n_bad_ck == 3, "counters: bad_ck = 3 (corrupt DBT, no-star, junk-after-HH)");
    CHECK(ctx.n_bad_fmt == 2, "counters: bad_fmt = 2 (HDT M, GGA garbage)");
    CHECK(ctx.n_unknown == 1, "counters: ZDA unknown");
}

static void test_rules(void)
{
    vessel_cells_t c = {0};
    vessel_judgment_t j;

    /* geofence band edges (box lat [57750000,57850000] lon [-152500000,-152350000] margin 5000) */
    c.fix = 1; c.lon_udeg = -152408333; c.sog_ukn = 0; c.depth_um = 0;
    c.lat_udeg = VESSEL_QM_GEOFENCE.lat_lo;
    vessel_judge(&c, &j);
    CHECK(j.geofence == VESSEL_SEV_WARN,
          "geofence: on the boundary -> watch (hugging the line)");
    c.lat_udeg = VESSEL_QM_GEOFENCE.lat_lo + VESSEL_QM_GEOFENCE.margin + 1;
    vessel_judge(&c, &j);
    CHECK(j.geofence == VESSEL_SEV_OK, "geofence: inside past the margin -> ok");
    c.lat_udeg = VESSEL_QM_GEOFENCE.lat_lo + 4000;   /* inside, near edge */
    vessel_judge(&c, &j);
    CHECK(j.geofence == VESSEL_SEV_WARN, "geofence: inside within margin -> warn");
    c.lat_udeg = VESSEL_QM_GEOFENCE.lat_lo - 4000;   /* outside, within margin */
    vessel_judge(&c, &j);
    CHECK(j.geofence == VESSEL_SEV_WARN, "geofence: outside within margin -> warn");
    c.lat_udeg = VESSEL_QM_GEOFENCE.lat_lo - 6000;   /* outside, past margin */
    vessel_judge(&c, &j);
    CHECK(j.geofence == VESSEL_SEV_BAD, "geofence: outside past margin -> bad");

    /* depth bands (alert < 5 m, warn < 10 m) */
    c.lat_udeg = 57798333; c.fix = 1;
    c.depth_um = 10000000; vessel_judge(&c, &j);
    CHECK(j.depth == VESSEL_SEV_OK, "depth: 10.0 m inclusive -> ok");
    c.depth_um = 9999999; vessel_judge(&c, &j);
    CHECK(j.depth == VESSEL_SEV_WARN, "depth: 9.999999 m -> warn");
    c.depth_um = 4999999; vessel_judge(&c, &j);
    CHECK(j.depth == VESSEL_SEV_BAD, "depth: 4.999999 m -> bad");
    c.depth_um = 0; vessel_judge(&c, &j);
    CHECK(j.depth == VESSEL_SEV_NA, "depth: 0 = no reading -> not judged");

    /* drift bands (warn > 0.5 kn, alert > 1.0 kn) */
    c.depth_um = 20000000;
    c.sog_ukn = 500000; vessel_judge(&c, &j);
    CHECK(j.drift == VESSEL_SEV_OK, "drift: 0.5 kn inclusive -> ok");
    c.sog_ukn = 500001; vessel_judge(&c, &j);
    CHECK(j.drift == VESSEL_SEV_WARN, "drift: 0.500001 kn -> warn");
    c.sog_ukn = 1000001; vessel_judge(&c, &j);
    CHECK(j.drift == VESSEL_SEV_BAD, "drift: 1.000001 kn -> bad");

    /* nofix + alert aggregation */
    c.sog_ukn = 0; c.fix = 0; vessel_judge(&c, &j);
    CHECK(j.nofix == VESSEL_QM_NOFIX_SEV && j.alert == VESSEL_SEV_WARN,
          "nofix: warn, alert aggregates");
    c.fix = 1; c.depth_um = 4200000; vessel_judge(&c, &j);
    CHECK(j.alert == VESSEL_SEV_BAD, "alert: worst rule wins (shallow)");
}

static void test_vm_wiring(void)
{
    qvm_t *vm;
    vessel_cells_t cells = {0};
    CHECK(vessel_limb_init(&vm, &cells) == 0, "wiring: limb init");

    /* cells are VM things whose values point INTO the cells struct */
    for (int i = 0; i < VESSEL_QM_N_CELLS; i++) {
        int32_t *v = (int32_t *)qm_view(vm, vessel_cell_names[i], "anyone");
        CHECK(v != NULL, "wiring: cell bound");
        if (!v) break;
        if (i == VESSEL_CELL_LAT)     CHECK(v == &cells.lat_udeg, "wiring: lat view is the cell");
        if (i == VESSEL_CELL_DEPTH)   CHECK(v == &cells.depth_um, "wiring: depth view is the cell");
    }

    /* lineage like blink/reflex */
    {
        qvm_thing_t *t = qvm_find(vm, "vessel-limb");
        CHECK(t && t->n_link_types == 1 && strcmp(t->link_types[0], "lineage") == 0
              && strcmp(t->link_targets[0][0], "vessel-root") == 0,
              "wiring: vessel-limb -lineage-> vessel-root");
    }

    /* absorb: sentence fields land in the sheet */
    nmea_ctx_t p; nmea_reset(&p);
    feed_str(&p, "$SDDBT,65.6,f,20.0,M,10.9,F*39");
    vessel_limb_absorb(&cells, &p.out);
    CHECK(cells.depth_um == 20000000
          && *(int32_t *)qm_view(vm, "own.depth", "anyone") == 20000000,
          "wiring: DBT absorbed, VIEW sees the sheet");

    /* serve: effect + tick land alert + canon response */
    vessel_judgment_t j;
    char resp[VESSEL_RESP_MAX];
    CHECK(vessel_limb_serve(vm, &cells, &j, resp) == 0, "wiring: serve rc");
    CHECK(strcmp(resp, "{\"alert\":1,\"depth\":0,\"drift\":0,\"geofence\":-1,\"nofix\":1}") == 0,
          "wiring: canon response (no fix -> geofence n/a, nofix warn, alert watch)");
    CHECK(j.alert == VESSEL_SEV_WARN, "wiring: alert = watch");
    {
        const vessel_judgment_t *a =
            (const vessel_judgment_t *)qm_view(vm, "vessel-limb:alert", "anyone");
        const char *r = (const char *)qm_view(vm, "vessel-limb:response", "anyone");
        CHECK(a && a->alert == j.alert && strcmp(r, resp) == 0,
              "wiring: VIEW reads back alert + response");
    }

    /* alert value replaced, not accumulated (steady-state heap) */
    feed_str(&p, "$GPGGA,120001,5747.900,N,15224.500,W,1,10,0.8,1.5,M,1.0,M,,*63");
    vessel_limb_absorb(&cells, &p.out);
    CHECK(vessel_limb_serve(vm, &cells, &j, resp) == 0
          && j.alert == VESSEL_SEV_OK && j.geofence == VESSEL_SEV_OK,
          "wiring: fix arrives -> geofence judges ok, alert clears");

    /* unknown target effect: the VM's own error, surfaced through canon */
    CHECK(qm_effect(vm, "no-such-thing", NULL, NULL, NULL) == -1,
          "wiring: EFFECT unknown target -> -1");

    vessel_limb_free(vm);
}

/* ── the demo: fixture stream → parser → sheet → alerts ────────────── */

/* expected outcome per stream line (index-parallel to the fixture):
 *   mode: 's' served (known core sentence) with expected alert level,
 *         'k' reject bad checksum, 'f' reject bad format,
 *         'u' unknown (accepted, not served), 'o' overflow drop */
typedef struct { char mode; int alert; } expect_t;

static const expect_t EXPECT[] = {
    { 's', 0 },  /*  0 GGA fix, in the working box            */
    { 's', 0 },  /*  1 RMC A, sog 0.3 kn                     */
    { 's', 0 },  /*  2 DBT 20.0 m                            */
    { 's', 0 },  /*  3 HDT 275.4                             */
    { 's', 0 },  /*  4 VHW 0.0 kn                            */
    { 's', 1 },  /*  5 DBT 8.0 m  -> shallow watch           */
    { 's', 2 },  /*  6 DBT 4.2 m  -> shallow ALERT           */
    { 's', 0 },  /*  7 DBT feet fallback 19.99 m             */
    { 's', 2 },  /*  8 GGA 58.5N  -> geofence ALERT          */
    { 's', 0 },  /*  9 GGA back inside                       */
    { 's', 2 },  /* 10 RMC sog 1.2 -> drift ALERT            */
    { 's', 0 },  /* 11 RMC sog 0.2                           */
    { 'k', 0 },  /* 12 DBT corrupt checksum -> rejected      */
    { 's', 1 },  /* 13 GGA fix 0 -> nofix watch              */
    { 's', 1 },  /* 14 RMC status V -> still nofix watch     */
    { 'u', 0 },  /* 15 ZDA time -> known=0, not served       */
    { 'o', 0 },  /* 16 GPTXT way past 82 chars -> dropped    */
    { 's', 1 },  /* 17 GGA 57.75005N inside near boundary    */
    { 's', 0 },  /* 18 GGA back to the middle                */
};
#define N_EXPECT ((int)(sizeof EXPECT / sizeof EXPECT[0]))

int main(int argc, char **argv)
{
    test_checksum();
    test_fixed_to_u();
    test_coords();
    test_sentences();
    test_rules();
    test_vm_wiring();
    printf("{\"unit\":\"%s\",\"passed\":%d,\"failed\":%d}\n",
           failed == 0 ? "ok" : "FAIL", passed, failed);

    /* ── the vessel demo ── */
    const char *path = argc > 1 ? argv[1] : "vessel-stream.txt";
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }

    qvm_t *vm;
    vessel_cells_t cells = {0};
    if (vessel_limb_init(&vm, &cells) != 0) {
        printf("{\"ok\":false,\"error\":\"limb init\"}\n");
        return 2;
    }
    printf("vessel-demo: %s — rules v%d sha %s\n", path, VESSEL_QM_VERSION,
           VESSEL_QM_SHA256);
    printf("geofence lat[%ld,%ld] lon[%ld,%ld]+%ldµdeg · depth alert<%ldµm warn<%ldµm · drift warn>%ldµkn alert>%ldµkn\n",
           (long)VESSEL_QM_GEOFENCE.lat_lo, (long)VESSEL_QM_GEOFENCE.lat_hi,
           (long)VESSEL_QM_GEOFENCE.lon_lo, (long)VESSEL_QM_GEOFENCE.lon_hi,
           (long)VESSEL_QM_GEOFENCE.margin,
           (long)VESSEL_QM_DEPTH_ALERT_BELOW, (long)VESSEL_QM_DEPTH_WARN_BELOW,
           (long)VESSEL_QM_DRIFT_WARN_ABOVE, (long)VESSEL_QM_DRIFT_ALERT_ABOVE);

    nmea_ctx_t ctx;
    nmea_reset(&ctx);
    char line[512];
    int i = 0, demo_fail = 0, served = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        /* '#' cannot appear in an NMEA payload (reserved set is $*!\) —
         * treat it as comment-to-end-of-line like the # headers */
        line[strcspn(line, "#")] = '\0';
        if (line[0] == '\0') continue;
        if (i >= N_EXPECT) break;

        /* strip expected-mode marker if present ("!k" / "!s2" style) is
         * NOT used: expectations live in EXPECT above, fixture is pure */
        int rc = feed_str(&ctx, line);

        if (rc == NMEA_DONE && ctx.out.known) {
            vessel_limb_absorb(&cells, &ctx.out);
            vessel_judgment_t j;
            char resp[VESSEL_RESP_MAX];
            vessel_limb_serve(vm, &cells, &j, resp);
            served++;
            const char *led = j.alert == 2 ? "red-flash" : j.alert == 1 ? "amber" : "green";
            printf("{\"i\":%d,\"addr\":\"%s\",\"lat\":%ld,\"lon\":%ld,\"sog\":%ld,\"depth\":%ld,"
                   "\"mode\":\"served\",\"response\":%s,\"led\":\"%s\"}\n",
                   i, ctx.out.addr, (long)cells.lat_udeg, (long)cells.lon_udeg,
                   (long)cells.sog_ukn, (long)cells.depth_um, resp, led);
            if (EXPECT[i].mode != 's' || EXPECT[i].alert != j.alert) {
                demo_fail++;
                printf("{\"i\":%d,\"demo\":\"MISMATCH\",\"want\":%c/%d,\"got\":\"s\"/%d}\n",
                       i, EXPECT[i].mode, EXPECT[i].alert, j.alert);
            }
        } else if (rc == NMEA_DONE) {
            printf("{\"i\":%d,\"addr\":\"%s\",\"mode\":\"unknown\"}\n", i, ctx.out.addr);
            if (EXPECT[i].mode != 'u') { demo_fail++; }
        } else if (rc == NMEA_BAD_CK) {
            printf("{\"i\":%d,\"mode\":\"bad-ck\"}\n", i);
            if (EXPECT[i].mode != 'k') { demo_fail++; }
        } else if (rc == NMEA_BAD_FMT) {
            printf("{\"i\":%d,\"mode\":\"bad-fmt\"}\n", i);
            if (EXPECT[i].mode != 'f') { demo_fail++; }
        } else if (rc == NMEA_OVERFLOW) {
            printf("{\"i\":%d,\"mode\":\"overflow\"}\n", i);
            if (EXPECT[i].mode != 'o') { demo_fail++; }
        }
        i++;
    }
    fclose(f);

    printf("{\"demo\":\"%s\",\"served\":%d,\"lines\":%d,"
           "\"ok\":%u,\"unknown\":%u,\"bad_ck\":%u,\"bad_fmt\":%u,\"overflow\":%u}\n",
           demo_fail == 0 && i == N_EXPECT ? "ok" : "FAIL",
           served, i, ctx.n_ok, ctx.n_unknown, ctx.n_bad_ck, ctx.n_bad_fmt,
           ctx.n_overflow);
    vessel_limb_free(vm);

    printf("{\"ok\":%s}\n", (failed == 0 && demo_fail == 0 && i == N_EXPECT)
           ? "true" : "false");
    return (failed == 0 && demo_fail == 0 && i == N_EXPECT) ? 0 : 1;
}
