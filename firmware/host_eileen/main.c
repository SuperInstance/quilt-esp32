/* host_eileen/main.c — THE EILEEN's metal lane on the desktop: unit
 * tests for the keel's beat, the stem's tally, the figurehead's listener,
 * and the dependency chain — each piece verified against the steel
 * sheet's formula semantics (eileen.sheet.yaml, transcribed below as an
 * INDEPENDENT oracle, not linked to the limb) — then the canon-verb
 * wiring, then the day replay: a fixture stream fed through the
 * byte-at-a-time parser, each completed sentence advancing the day, the
 * chain evaluated and verified piece by piece, keel→figurehead, in joint
 * order. One JSON line per sentence, one summary, exit 0 iff all green.
 *
 * This is the four-material verification: prose manifest ↔ steel YAML ↔
 * metal .qm ↔ bread tempo, row by row. See
 * docs/creative/the-eileen-metal-verification.md for the agreement table.
 *
 *   ./host_eileen_bin [eileen-days.txt]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmea.h"
#include "eileen_limb.h"

static int passed = 0, failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { passed++; } \
    else { failed++; printf("{\"fail\":\"%s\"}\n", name); } \
} while (0)

/* feed a full line (adds '\n') */
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

/* ── the steel oracle ────────────────────────────────────────────────
 * eileen.sheet.yaml's expr strings, transcribed one for one. The limb
 * never sees this code — agreement between the two IS the test. */
static void steel_eval(int32_t stem, eileen_cells_t *out)
{
    out->keel = 1;                                        /* value: 1    */
    out->stem = stem;                                     /* sensor      */
    out->keelson = out->stem + out->keel;                 /* =stem+keel  */
    out->breast_hook = out->keelson * 2;                  /* =keelson*2  */
    out->rigging = out->breast_hook + out->keelson;       /* =bh+keelson */
    out->bulwarks = out->rigging - 2;                     /* =rigging-2  */
    out->ensign = out->bulwarks + 1;                      /* =bulwarks+1 */
    out->scuppers = out->ensign - 1;                      /* =ensign-1   */
    out->sheerboard = out->scuppers + out->breast_hook;   /* =scu+bh     */
    out->figurehead = out->sheerboard > 0 ? out->keel : 0; /* listener   */
}

static int cells_agree(const eileen_cells_t *a, const eileen_cells_t *b)
{
    return a->keel == b->keel && a->stem == b->stem
        && a->keelson == b->keelson && a->breast_hook == b->breast_hook
        && a->rigging == b->rigging && a->bulwarks == b->bulwarks
        && a->ensign == b->ensign && a->scuppers == b->scuppers
        && a->sheerboard == b->sheerboard && a->figurehead == b->figurehead;
}

/* ── unit tests ────────────────────────────────────────────────────── */

static void test_keel(void)
{
    /* the keel is laid first: value 1 before any water arrives */
    eileen_cells_t c = {0};
    qvm_t *vm = NULL;
    CHECK(eileen_limb_init(&vm, &c) == 0, "keel: limb init");
    CHECK(c.keel == 1, "keel: laid true — value 1 at init");
    CHECK(c.stem == 0, "keel: no water yet, stem 0");
    eileen_limb_free(vm);

    /* ♩=60: half a second lit, half a second dark — blink.qm's table,
     * the 13:19 blink, movement I of the bread score */
    CHECK(EILEEN_KEEL_BLINK_MS == 500, "keel: beat = 500 ms");
    CHECK(eileen_keel_phase(0) == 1, "keel: phase lit at t=0");
    CHECK(eileen_keel_phase(499) == 1, "keel: lit through t=499");
    CHECK(eileen_keel_phase(500) == 0, "keel: dark at t=500");
    CHECK(eileen_keel_phase(999) == 0, "keel: dark through t=999");
    CHECK(eileen_keel_phase(1000) == 1, "keel: lit again at t=1000 — period 1 s");
    CHECK(eileen_keel_phase(1500) == 0, "keel: dark at t=1500");
    CHECK(eileen_keel_phase(4000) == 1 && eileen_keel_phase(4500) == 0,
          "keel: steady tempo, year ten");
}

static void test_absorb(void)
{
    nmea_ctx_t p;
    eileen_cells_t c = {0};
    nmea_reset(&p);

    /* a completed sentence advances the day */
    CHECK(feed_str(&p, "$GPGGA,120001,5747.900,N,15224.500,W,1,10,0.8,1.5,M,1.0,M,,*63") == NMEA_DONE,
          "stem: GGA survives the tally");
    eileen_limb_absorb(&c, &p.out);
    CHECK(c.stem == 1, "stem: day 1 after the first sentence");

    CHECK(feed_str(&p, "$HCHDT,275.4,T*2D") == NMEA_DONE,
          "stem: HDT survives");
    eileen_limb_absorb(&c, &p.out);
    CHECK(c.stem == 2, "stem: day 2");

    /* the tally fails: the whole line goes over the side — the caller
     * does not absorb rejects; nothing else to test there */
    CHECK(feed_str(&p, "$SDDBT,26.2,f,8.0,M,4.4,F*00") == NMEA_BAD_CK,
          "stem: corrupt checksum goes over the side");
    CHECK(c.stem == 2, "stem: a rejected line does not advance the day");

    /* unknown type, valid checksum: the water was true, it counts */
    CHECK(feed_str(&p, "$GPZDA,160012.71,11,03,2004,-1,00*7D") == NMEA_DONE
          && !p.out.known, "stem: ZDA valid but unknown");
    eileen_limb_absorb(&c, &p.out);
    CHECK(c.stem == 3, "stem: what survives the tally advances the day");

    /* saturation: the integer-discipline wall */
    c.stem = EILEEN_QM_DAY_MAX;
    eileen_limb_absorb(&c, &p.out);
    CHECK(c.stem == EILEEN_QM_DAY_MAX, "stem: day saturates, wood cannot overflow");
    c.stem = EILEEN_QM_DAY_MAX - 1;
    eileen_limb_absorb(&c, &p.out);
    CHECK(c.stem == EILEEN_QM_DAY_MAX, "stem: saturates exactly at the ceiling");
    CHECK(5LL * EILEEN_QM_DAY_MAX + 3 <= 2147483647LL,
          "stem: sheerboard=5N+3 fits int32 at the ceiling");
}

static void test_figurehead(void)
{
    CHECK(eileen_figurehead_fires(0) == 0, "figurehead: holds at 0");
    CHECK(eileen_figurehead_fires(1) == 1, "figurehead: fires above 0");
    CHECK(eileen_figurehead_fires(-1) == 0, "figurehead: holds below 0");
    CHECK(eileen_figurehead_fires(2147483647) == 1,
          "figurehead: fires at the rail");
}

static void test_chain(void)
{
    /* the joint order is the frozen order: keel, stem, keelson, … */
    for (int i = 0; i < EILEEN_QM_N_CELLS; i++) {
        CHECK(EILEEN_QM_JOINTS[i] == i, "chain: joint order frozen keel→figurehead");
    }
    CHECK(strcmp(eileen_cell_names[0], "keel") == 0
          && strcmp(eileen_cell_names[9], "figurehead") == 0,
          "chain: the ten pieces named as the manifest names them");

    /* each day: the chain must equal the steel sheet, piece by piece,
     * and the .qm's closed form, line by line */
    static const int32_t days[] = { 0, 1, 2, 3, 4, 5, 7, 10, 100, 1000, 100000 };
    for (size_t k = 0; k < sizeof days / sizeof days[0]; k++) {
        int32_t N = days[k];
        eileen_cells_t c = {0};
        c.keel = 1; c.stem = N;
        eileen_day_t day;
        eileen_chain(&c, &day);

        eileen_cells_t oracle;
        steel_eval(N, &oracle);
        char name[64];
        snprintf(name, sizeof name, "chain: day %ld agrees with the steel sheet",
                 (long)N);
        CHECK(cells_agree(&day.v, &oracle), name);

        /* closed form (eileen.qm integer_discipline, third rendering) */
        snprintf(name, sizeof name, "chain: day %ld closed form", (long)N);
        CHECK(day.v.keelson == N + 1 && day.v.breast_hook == 2 * N + 2
              && day.v.rigging == 3 * N + 3 && day.v.bulwarks == 3 * N + 1
              && day.v.ensign == 3 * N + 2 && day.v.scuppers == 3 * N + 1
              && day.v.sheerboard == 5 * N + 3
              && day.v.figurehead == 1 && day.v.keel == 1 && day.v.stem == N,
              name);

        snprintf(name, sizeof name, "chain: day %ld evaluation order = joints",
                 (long)N);
        int ord_ok = day.day == N && day.fired == 1;
        for (int i = 0; i < EILEEN_QM_N_CELLS; i++)
            ord_ok = ord_ok && day.order[i] == EILEEN_QM_JOINTS[i];
        CHECK(ord_ok, name);
    }

    /* the figurehead closes on the keel's value, not on her own watch:
     * with a doctored sheerboard of 0 the listener holds — the pure
     * condition, checked straight (the chain itself always sheers > 0
     * once the keel is laid: 5N+3 >= 3, which is the point) */
    CHECK(eileen_figurehead_fires(0) == 0,
          "chain: no sheer, no closing — the listener waits");
}

static void test_vm_wiring(void)
{
    qvm_t *vm;
    eileen_cells_t cells;
    CHECK(eileen_limb_init(&vm, &cells) == 0, "wiring: limb init");

    /* cells are VM things whose values point INTO the cells struct */
    for (int i = 0; i < EILEEN_QM_N_CELLS; i++) {
        int32_t *v = (int32_t *)qm_view(vm, eileen_cell_names[i], "anyone");
        CHECK(v != NULL, "wiring: piece bound");
        if (!v) break;
        if (i == EILEEN_CELL_KEEL)       CHECK(v == &cells.keel, "wiring: keel view is the cell");
        if (i == EILEEN_CELL_STEM)       CHECK(v == &cells.stem, "wiring: stem view is the cell");
        if (i == EILEEN_CELL_FIGUREHEAD) CHECK(v == &cells.figurehead, "wiring: figurehead view is the cell");
    }

    /* lineage like blink/reflex/nmea */
    {
        qvm_thing_t *t = qvm_find(vm, "eileen-limb");
        CHECK(t && t->n_link_types == 1 && strcmp(t->link_types[0], "lineage") == 0
              && strcmp(t->link_targets[0][0], "eileen-root") == 0,
              "wiring: eileen-limb -lineage-> eileen-root");
    }

    /* facts carry the mint receipt */
    {
        const char *facts = (const char *)qm_view(vm, "eileen-limb:facts", "anyone");
        CHECK(facts && strstr(facts, EILEEN_QM_SHA256) != NULL,
              "wiring: facts cite the receipt sha");
    }

    /* log.figurehead: the closing line, bound as canon */
    {
        const char *log = (const char *)qm_view(vm, "log.figurehead", "anyone");
        CHECK(log && strcmp(log, "\"she faces forward; the days grew from the keel\"") == 0,
              "wiring: log.figurehead closes the work where it began");
    }

    /* serve: effect + tick land the day + canon response */
    nmea_ctx_t p; nmea_reset(&p);
    feed_str(&p, "$GPGGA,120001,5747.900,N,15224.500,W,1,10,0.8,1.5,M,1.0,M,,*63");
    eileen_limb_absorb(&cells, &p.out);
    eileen_day_t day;
    char resp[EILEEN_RESP_MAX];
    CHECK(eileen_limb_serve(vm, &cells, &day, resp) == 0, "wiring: serve rc");
    CHECK(strcmp(resp,
          "{\"breast_hook\":4,\"bulwarks\":4,\"day\":1,\"ensign\":5,"
          "\"figurehead\":1,\"fired\":1,\"keel\":1,\"keelson\":2,"
          "\"rigging\":6,\"scuppers\":4,\"sheerboard\":8,\"stem\":1}") == 0,
          "wiring: canon response day 1 (sorted keys, integers only)");
    {
        const eileen_day_t *d = (const eileen_day_t *)qm_view(vm, "eileen-limb:day", "anyone");
        const char *r = (const char *)qm_view(vm, "eileen-limb:response", "anyone");
        CHECK(d && d->day == 1 && d->fired == 1 && d->v.sheerboard == 8
              && strcmp(r, resp) == 0,
              "wiring: VIEW reads back day + response");
    }

    /* the next day replaces, not accumulates (steady-state heap) */
    feed_str(&p, "$HCHDT,275.4,T*2D");
    eileen_limb_absorb(&cells, &p.out);
    CHECK(eileen_limb_serve(vm, &cells, &day, resp) == 0
          && day.day == 2 && day.v.keelson == 3 && day.v.sheerboard == 13
          && day.fired == 1,
          "wiring: day 2 serves — the days grew from the keel");

    /* unknown target effect: the VM's own error, surfaced through canon */
    CHECK(qm_effect(vm, "no-such-thing", NULL, NULL, NULL) == -1,
          "wiring: EFFECT unknown target -> -1");

    eileen_limb_free(vm);
}

/* ── the day replay: fixture stream → stem → chain, verified ───────── */

/* expected outcome per stream line (index-parallel to the fixture):
 *   'd' = completed sentence, day advances (known or unknown — what
 *         survives the tally counts)
 *   'x' = rejected (bad checksum / bad format), no advance */
typedef struct { char mode; } expect_t;

static const expect_t EXPECT[] = {
    { 'd' },  /*  0 GGA fix Kodiak          -> day 1  */
    { 'd' },  /*  1 RMC A, sog 0.3          -> day 2  */
    { 'd' },  /*  2 DBT 20.0 m              -> day 3  */
    { 'x' },  /*  3 DBT corrupt checksum    -> over the side */
    { 'd' },  /*  4 HDT 275.4               -> day 4  */
    { 'x' },  /*  5 HDT 'M' where T belongs -> over the side */
    { 'd' },  /*  6 VHW 0.0 kn              -> day 5  */
    { 'd' },  /*  7 DBT 4.2 m               -> day 6  */
    { 'd' },  /*  8 GGA 58.5N               -> day 7  */
    { 'd' },  /*  9 ZDA unknown, valid ck   -> day 8  */
    { 'd' },  /* 10 GGA back inside         -> day 9  */
    { 'd' },  /* 11 RMC sog 1.2             -> day 10 */
    { 'x' },  /* 12 RMC corrupt             -> over the side */
};
#define N_EXPECT ((int)(sizeof EXPECT / sizeof EXPECT[0]))

int main(int argc, char **argv)
{
    test_keel();
    test_absorb();
    test_figurehead();
    test_chain();
    test_vm_wiring();
    printf("{\"unit\":\"%s\",\"passed\":%d,\"failed\":%d}\n",
           failed == 0 ? "ok" : "FAIL", passed, failed);

    /* ── the day replay ── */
    const char *path = argc > 1 ? argv[1] : "eileen-days.txt";
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }

    qvm_t *vm;
    eileen_cells_t cells;
    if (eileen_limb_init(&vm, &cells) != 0) {
        printf("{\"ok\":false,\"error\":\"limb init\"}\n");
        return 2;
    }
    printf("the-eileen day replay: %s — vessel v%d sha %s\n", path,
           EILEEN_QM_VERSION, EILEEN_QM_SHA256);
    printf("joints: keel stem keelson breast_hook rigging bulwarks ensign "
           "scuppers sheerboard figurehead — closed on the keel\n");

    nmea_ctx_t ctx;
    nmea_reset(&ctx);
    char line[512];
    int i = 0, demo_fail = 0, days = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        line[strcspn(line, "#")] = '\0';   /* comments (reserved-set arg) */
        if (line[0] == '\0') continue;
        if (i >= N_EXPECT) break;

        int rc = feed_str(&ctx, line);

        if (rc == NMEA_DONE) {
            eileen_limb_absorb(&cells, &ctx.out);   /* the day advances */
            eileen_day_t day;
            char resp[EILEEN_RESP_MAX];
            if (eileen_limb_serve(vm, &cells, &day, resp) != 0) {
                demo_fail++;
                printf("{\"i\":%d,\"mode\":\"serve-error\"}\n", i);
            } else {
                days++;
                /* the four-material row check: steel vs metal, this day */
                eileen_cells_t oracle;
                steel_eval(day.v.stem, &oracle);
                int agree = cells_agree(&day.v, &oracle);
                int ord_ok = 1;
                for (int j = 0; j < EILEEN_QM_N_CELLS; j++)
                    ord_ok = ord_ok && day.order[j] == EILEEN_QM_JOINTS[j];
                if (!agree || !ord_ok) demo_fail++;
                printf("{\"i\":%d,\"addr\":\"%s\",\"mode\":\"day\","
                       "\"day\":%ld,\"sheerboard\":%ld,\"figurehead\":%ld,"
                       "\"fired\":%d,\"agree\":%s}\n",
                       i, ctx.out.addr, (long)day.day, (long)day.v.sheerboard,
                       (long)day.v.figurehead, day.fired,
                       (agree && ord_ok) ? "true" : "false");
            }
            if (EXPECT[i].mode != 'd') demo_fail++;
        } else if (rc == NMEA_BAD_CK || rc == NMEA_BAD_FMT || rc == NMEA_OVERFLOW) {
            printf("{\"i\":%d,\"mode\":\"%s\"}\n", i,
                   rc == NMEA_BAD_CK ? "overboard-bad-ck"
                   : rc == NMEA_BAD_FMT ? "overboard-bad-fmt" : "overboard-overflow");
            if (EXPECT[i].mode != 'x') demo_fail++;
        }
        i++;
    }
    fclose(f);

    printf("{\"replay\":\"%s\",\"lines\":%d,\"days\":%d,"
           "\"ok\":%u,\"unknown\":%u,\"bad_ck\":%u,\"bad_fmt\":%u}\n",
           demo_fail == 0 && i == N_EXPECT ? "ok" : "FAIL",
           i, days, ctx.n_ok, ctx.n_unknown, ctx.n_bad_ck, ctx.n_bad_fmt);
    eileen_limb_free(vm);

    if (failed == 0 && demo_fail == 0 && i == N_EXPECT) {
        printf("log.figurehead: she faces forward; the days grew from the keel\n");
    }
    printf("{\"ok\":%s}\n", (failed == 0 && demo_fail == 0 && i == N_EXPECT)
           ? "true" : "false");
    return (failed == 0 && demo_fail == 0 && i == N_EXPECT) ? 0 : 1;
}
