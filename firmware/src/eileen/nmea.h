/* nmea.h — NMEA 0183 sentence parser for the vessel limb (host + target).
 *
 * The boat's instruments speak NMEA 0183 over serial: $GPGGA… (fix),
 * $GPRMC… (position/course/speed), $HCHDT… (true heading), $SDDBT…
 * (depth below transducer), $VWVHW… (water speed). This file turns that
 * byte stream into integer micro-unit fields — no floats anywhere, on
 * host or metal (the reflex-arc discipline; see critic_gate.h).
 *
 * DESIGN CONTRACT (the walls this parser lives inside):
 *   - byte-at-a-time: nmea_feed(ctx, byte) — ISR/UART-friendly, O(1) per
 *     byte, no second pass, checksum accumulated incrementally;
 *   - NO malloc on target: the line buffer is a fixed 96-byte array in
 *     the context struct. NMEA 0183 caps a sentence at 82 chars
 *     including '$' and CRLF; 96 gives margin without inviting abuse.
 *     Longer lines (rare AIS multi-fragment padding, junk) are dropped
 *     whole and counted in n_overflow — the parser resyncs on '$';
 *   - framing: garbage before '$' is discarded; '$' opens a sentence;
 *     '\r' ignored, '\n' closes it; a '$' mid-sentence restarts capture
 *     (resync). Checksum is REQUIRED (NMEA 0183 4.x): a sentence without
 *     "*HH" is rejected and counted in n_bad_ck;
 *   - checksum: XOR of all bytes strictly between '$' and '*', compared
 *     against the two hex digits after '*' (case-insensitive read).
 *
 * FIXED-POINT CONVERSIONS (documented, exact where the grid allows):
 *   - coordinates  ddmm.mmmm / dddmm.mmmm  -> signed micro-degrees
 *     (µdeg, 1e-6 °). dd = degrees, mm.mmmm = minutes; 64-bit integer
 *     intermediate: µdeg = dd*1e6 + (mm*10^f + frac)*1e6 / (60*10^f).
 *     Max |value| 180e6 fits int32; precision ±1 µdeg ≈ 0.11 m;
 *   - speed over ground / water speed: knots "022.4" -> micro-knots;
 *   - course/heading "084.4"            -> micro-degrees;
 *   - depth DBT: meters field -> micro-meters; if the meters field is
 *     empty, feet field converts exactly: µm = µft * 3048 / 10000
 *     (0.3048 m/ft exact on that grid).
 *
 * C99 + stdint only; no POSIX, no stdio, no allocation.
 */
#ifndef NMEA_H
#define NMEA_H

#include <stddef.h>
#include <stdint.h>

/* ── limits ── */
#define NMEA_MAX_LINE 96  /* spec max 82 incl $ + CRLF; +14 margin */

/* ── nmea_feed return codes ── */
#define NMEA_MORE 0        /* byte consumed, sentence not complete yet  */
#define NMEA_DONE 1        /* complete VALID sentence, ctx->out filled  */
#define NMEA_OVERFLOW (-1) /* line longer than NMEA_MAX_LINE: dropped   */
#define NMEA_BAD_CK (-2)   /* complete line, checksum failed/absent     */
#define NMEA_BAD_FMT (-3)  /* valid checksum, unparseable payload      */

/* severity-style: field validity comes from where/known flags below */

typedef struct {
    char addr[6];          /* address field: talker+type, e.g. "GPGGA"  */
    int  known;            /* 1 = one of the core five, fields parsed   */
    /* GGA (fix) + RMC (recommended minimum) */
    int  has_fix;          /* 1 = the sentence carries a usable fix     */
    int  fix_quality;      /* GGA field 6 (0=invalid,1=GPS,2=DGPS…)     */
    int  n_sat;            /* GGA field 7, satellites used              */
    int32_t lat_udeg;      /* GGA/RMC, signed micro-degrees (N/E +)     */
    int32_t lon_udeg;
    int32_t sog_ukn;       /* RMC field 7, micro-knots                  */
    int32_t cog_udeg;      /* RMC field 8, micro-degrees                */
    /* HDT (true heading) */
    int32_t heading_udeg;  /* HDT field 1, micro-degrees                */
    /* DBT (depth below transducer) */
    int32_t depth_um;      /* DBT field 3 (meters; else feet converted) */
    /* VHW (water speed) */
    int32_t wspeed_ukn;    /* VHW field 5 knots (else kph field 7)      */
} nmea_sentence_t;

typedef struct {
    /* line assembly (raw sentence kept unmutated for echo/log) */
    char   line[NMEA_MAX_LINE];
    size_t len;
    uint8_t xor_acc;       /* checksum accumulated between $ and *      */
    int    got_star;       /* 1 after '*', expecting 2 hex digits       */
    int    ck_idx;         /* 0,1,2: how many hex digits seen           */
    uint8_t ck_given;      /* the two hex digits, packed                */
    int    overflow;       /* line exceeded the buffer: drop until \n   */

    /* the last fully-parsed sentence (valid when feed returns DONE) */
    nmea_sentence_t out;

    /* counters (uint32: a season of 4800-baud traffic fits) */
    uint32_t n_ok;         /* valid core sentences parsed               */
    uint32_t n_unknown;    /* valid checksum, not a core type           */
    uint32_t n_bad_ck;     /* checksum failed or absent                 */
    uint32_t n_bad_fmt;    /* checksum ok, payload unparseable          */
    uint32_t n_overflow;   /* lines dropped for length                  */
} nmea_ctx_t;

/* initialize/reset a context (counters zeroed, no sentence in progress) */
void nmea_reset(nmea_ctx_t *ctx);

/* Feed one byte of the UART stream. Returns one of the NMEA_* codes
 * above. On NMEA_DONE, ctx->out holds the parsed sentence (out.known
 * says whether the core fields were filled) and ctx->line[0..len) holds
 * the raw sentence text without CR/LF. On NMEA_OVERFLOW the whole line
 * is being discarded (one return per dropped line); the parser keeps
 * consuming until '\n', then resyncs on the next '$'. */
int nmea_feed(nmea_ctx_t *ctx, char c);

/* ── exposed for tests and reuse: integer converters ──
 * All take a non-NUL-terminated field (ptr, len) straight from the line
 * buffer. Return 0 ok, -1 malformed/out-of-range. */

/* "$..*HH" checksum check on a COMPLETE raw line (ptr[0] must be '$').
 * Returns 0 if the trailing *HH matches the XOR of the body, -1 else
 * (including: no '*', bad hex, trailing junk after HH). Case-insensitive
 * hex. (nmea_feed uses the incremental accumulator; this is the
 * whole-line form tests and the host replay harness use.) */
int nmea_checksum_ok(const char *line, size_t len);

/* "022.4" / "18.0" / "275.4" -> value in micro-units (1e-6). Sign not
 * accepted (NMEA magnitude fields are unsigned; hemisphere is separate).
 * More than 6 fraction digits truncate (µ grid). Integer part capped at
 * 2000 (largest legal value here is depth in meters; anything past is
 * instrument babble). */
int nmea_fixed_to_u(const char *f, size_t len, int32_t *out_u);

/* "4807.038" + 'N' -> 48117300 µdeg; 'S'/'W' negate. Validates mm<=59,
 * |deg|<=90 (lat when is_lon==0) / <=180 (is_lon==1), total |µdeg| in
 * range. 64-bit integer intermediate inside — no floats. */
int nmea_coord_to_udeg(const char *f, size_t len, char hemi,
                       int is_lon, int32_t *out_udeg);

#endif /* NMEA_H */
