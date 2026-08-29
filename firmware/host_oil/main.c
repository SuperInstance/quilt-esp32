/* host_oil/main.c -- the TWIN-PORT LANE on the desktop: the same 17
 * hand-computed golden vectors quilt-verilog's tools/tower/verify.py
 * gates the generator on, run against THIS repo's copy of the generated
 * cell (cells/oil-pressure/tower_oil_pressure_port.c) -- the rendering
 * chain minus ADC glue, driven through the weak-seam overrides exactly
 * like the on-target build drives it. Law 5 (FOUNDATON 4): verification
 * is the judgment that two interpretations of one cell agree -- here,
 * the upstream golden table and this repo's compiled cell.
 *
 * Staged per section (mirrors verify.py):
 *   1. the 17-row golden (voltage -> PSI) table, whole-unit exact, both
 *      clamp edges included;
 *   2. the moving median rejects a single-sample spike exactly;
 *   3. the deadband judge is a Schmitt trigger: WITHIN (no post) at
 *      d = D, SNAP (post, reality wins, debt booked) at d = D + 1;
 *   4. the QUF state line carries the final image, all values integer,
 *      and the endpoint string survives the transport seam.
 *
 *   make oil-run     (gcc host build, exit 0 iff all green)
 */
#include <stdio.h>
#include <string.h>

#include "tower_port.h"

static int passed = 0, failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { passed++; } \
    else { failed++; printf("FAIL %s\n", name); } \
} while (0)

/* host-side seam overrides (the on-target build supplies the same two
 * functions from board glue: ADC adapter + twin transport) */
static int32_t g_mV = 0;
static unsigned g_posts = 0;
static char g_last_ep[128];
static char g_last_line[256];

int32_t tower_adc_read_mV(void) { return g_mV; }

void tower_twin_post(const char *endpoint, const char *state_line)
{
    snprintf(g_last_ep, sizeof g_last_ep, "%s", endpoint);
    snprintf(g_last_line, sizeof g_last_line, "%s", state_line);
    g_posts++;
}

static void feed(int32_t mV, int n)
{
    int i;
    g_mV = mV;
    for (i = 0; i < n; i++) {
        tower_tick();
    }
}

static void reset_cell(void)
{
    tower_reset();
    g_posts = 0;
    g_last_ep[0] = '\0';
    g_last_line[0] = '\0';
}

/* the 17 hand-computed golden anchors (verify.py HAND_GOLDEN, verbatim):
 * on-lattice rows every 400 mV (exactly 15 psi apart on the 1/80-psi
 * basis), off-lattice rows exercising round-to-nearest, and both clamp
 * edges of the [0,150] psi range. */
struct row { int mV; int psi; };
static const struct row GOLD[] = {
    {500, 0}, {900, 15}, {1300, 30}, {1700, 45}, {2100, 60},
    {2500, 75}, {2900, 90}, {3300, 105}, {3700, 120}, {4100, 135},
    {4500, 150},
    {750, 9}, {1000, 19}, {2750, 84}, {4499, 150}, {475, 0}, {4600, 150},
};

/* independent expectation for the whole-psi lattice points the Schmitt
 * section needs (hand math on the cell equation, not the cell's code) */
#define PSI_AT_1000 19  /* (1000-500)*3 = 1500; (1500+40)/80 = 19 */

int main(void)
{
    size_t k;
    char line[256];
    size_t n;

    printf("host_oil -- twin-port lane, 17 golden vectors on the ported cell\n");

    /* 1. golden rendering table, whole-unit exact */
    for (k = 0; k < sizeof GOLD / sizeof GOLD[0]; k++) {
        char what[48];
        reset_cell();
        feed(GOLD[k].mV, 5); /* window warm: median of 5 identical samples */
        snprintf(what, sizeof what, "golden mV=%d -> %d psi",
                 GOLD[k].mV, GOLD[k].psi);
        CHECK(tower_psi_whole() == GOLD[k].psi, what);
    }
    printf("  golden   : %zu voltage->PSI rows run\n",
           sizeof GOLD / sizeof GOLD[0]);

    /* 2. median rejects a single spike */
    reset_cell();
    feed(1000, 5);
    CHECK(tower_psi_whole() == PSI_AT_1000, "median warm at 1000 mV");
    feed(4500, 1); /* one ignition spike */
    CHECK(tower_psi_whole() == PSI_AT_1000, "median rejects 4500 mV spike");
    feed(1000, 1);
    CHECK(tower_psi_whole() == PSI_AT_1000, "median stable after spike");

    /* 3. deadband Schmitt: d == D holds, d == D+1 snaps (D = 1 psi) */
    reset_cell();
    feed(2500, 5);
    CHECK(g_posts == 1, "initial sync posts once");
    CHECK(tower_twin_psi() == 75, "twin synced to 75 psi");
    tower_twin_set_belief(74);
    feed(2500, 1);
    CHECK(g_posts == 1, "WITHIN at d=1 (=D): no post");
    tower_twin_set_belief(77);
    feed(2500, 1);
    CHECK(g_posts == 2, "SNAP at d=2 (>D): posts");
    CHECK(tower_twin_psi() == 75, "reality wins: twin back to 75");
    CHECK(tower_snap_debt() == 2, "snap debt books +2 psi");
    tower_twin_set_belief(76);
    feed(2500, 1);
    CHECK(g_posts == 2, "WITHIN at d=1 again: no post");

    /* 4. endpoint + QUF state line: the whole final image, integers only */
    CHECK(strcmp(g_last_ep, "twin://game/oil-pressure-port/psi") == 0,
          "endpoint carried through the transport seam");
    n = tower_quf_line(line, sizeof line);
    CHECK(n == strlen(line), "quf line nul-terminated at reported length");
    CHECK(strcmp(line,
        "QUF1 cell=oil-pressure-port tick=8 raw_mV=2500 med_mV=2500"
        " psi80=6000 psi=75 twin=76 deadband_psi=1 posts=2 snap_debt=2") == 0,
        "quf line exact final image");

    printf("SUMMARY passed=%d failed=%d golden_rows=%zu\n",
           passed, failed, sizeof GOLD / sizeof GOLD[0]);
    if (failed == 0) {
        printf("oil-pressure lane: PORTED CELL AGREES WITH THE TOWER -- PASS\n");
    }
    return failed ? 1 : 0;
}
