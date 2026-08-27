/* nmea.c — NMEA 0183 parser, portable C99, no malloc, no floats, no stdio.
 * Byte-at-a-time state machine (UART/ISR friendly): '$' opens a sentence,
 * CR ignored, '\n' closes it, checksum XOR accumulated between '$' and
 * '*' as bytes arrive. Host harness and ESP32 firmware compile THIS file
 * unchanged — same discipline as critic_gate.c in reflex-arc.
 *
 * Sentence set (the core marine five): GGA, RMC, HDT, DBT, VHW. Valid
 * checksum + unknown type counts n_unknown (accepted, no fields); valid
 * checksum + malformed core payload counts n_bad_fmt (rejected).
 */
#include "nmea.h"

/* ── small integer helpers ─────────────────────────────────────────── */

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* unsigned integer from (ptr,len); -1 on any non-digit or >9 digits */
static long long parse_uint(const char *p, size_t len)
{
    if (len == 0 || len > 9) return -1;
    long long v = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] < '0' || p[i] > '9') return -1;
        v = v * 10 + (p[i] - '0');
    }
    return v;
}

int nmea_fixed_to_u(const char *f, size_t len, int32_t *out_u)
{
    size_t dot = 0;
    int seen_dot = 0;
    for (size_t i = 0; i < len; i++) {
        if (f[i] == '.') {
            if (seen_dot) return -1;
            seen_dot = 1;
            dot = i;
        } else if (f[i] < '0' || f[i] > '9') {
            return -1;
        }
    }
    size_t ip_len = seen_dot ? dot : len;
    long long ip = parse_uint(f, ip_len);
    if (ip < 0 || ip > 2000) return -1;         /* sanity cap, see header */
    size_t fr_len = seen_dot ? len - dot - 1 : 0;
    if (fr_len > 6) fr_len = 6;                 /* truncate past the µ grid:
                                                   extra digits dropped */
    long long fr = 0;
    for (size_t i = 0; i < fr_len; i++)
        fr = fr * 10 + (f[dot + 1 + i] - '0');
    long long scale = 1;
    for (size_t i = 0; i < fr_len; i++) scale *= 10;
    /* µ = ip*1e6 + fr*1e6/scale  (fits: ip<=2000 -> 2e9 boundary-checked) */
    long long u = ip * 1000000LL + fr * 1000000LL / scale;
    if (u > 2147483647LL) return -1;
    *out_u = (int32_t)u;
    return 0;
}

int nmea_coord_to_udeg(const char *f, size_t len, char hemi,
                       int is_lon, int32_t *out_udeg)
{
    if (hemi != 'N' && hemi != 'S' && hemi != 'E' && hemi != 'W') return -1;
    if ((hemi == 'N' || hemi == 'S') && is_lon) return -1;
    if ((hemi == 'E' || hemi == 'W') && !is_lon) return -1;

    size_t dot = len;                            /* allow no-decimal form */
    int seen_dot = 0;
    for (size_t i = 0; i < len; i++) {
        if (f[i] == '.') {
            if (seen_dot) return -1;
            seen_dot = 1;
            dot = i;
        } else if (f[i] < '0' || f[i] > '9') {
            return -1;
        }
    }
    size_t ip_len = seen_dot ? dot : len;
    if (ip_len < 3 || ip_len > 5) return -1;     /* ddm / dddmm */

    long long deg = parse_uint(f, ip_len - 2);
    long long mm  = parse_uint(f + ip_len - 2, 2);
    if (deg < 0 || mm < 0) return -1;
    long long max_deg = is_lon ? 180 : 90;
    if (deg > max_deg || mm > 59) return -1;

    size_t fr_len = seen_dot ? len - dot - 1 : 0;
    if (fr_len > 6) fr_len = 6;
    long long fr = 0;
    for (size_t i = 0; i < fr_len; i++)
        fr = fr * 10 + (f[dot + 1 + i] - '0');
    long long scale = 1;
    for (size_t i = 0; i < fr_len; i++) scale *= 10;

    /* µdeg = deg*1e6 + (mm*scale + fr) * 1e6 / (60*scale), 64-bit exact */
    long long minutes_scaled = mm * scale + fr;
    long long u = deg * 1000000LL + minutes_scaled * 1000000LL / (60LL * scale);
    if (u > max_deg * 1000000LL) return -1;      /* e.g. 9030.000,N */

    if (hemi == 'S' || hemi == 'W') u = -u;
    *out_udeg = (int32_t)u;
    return 0;
}

int nmea_checksum_ok(const char *line, size_t len)
{
    if (len < 4 || line[0] != '$') return -1;
    size_t star = len;
    for (size_t i = 1; i < len; i++)
        if (line[i] == '*') { star = i; break; }
    if (star == len || star + 2 >= len || star + 3 != len) return -1;
    int hi = hex_val(line[star + 1]), lo = hex_val(line[star + 2]);
    if (hi < 0 || lo < 0) return -1;
    uint8_t xor = 0;
    for (size_t i = 1; i < star; i++) xor ^= (uint8_t)line[i];
    return xor == (uint8_t)((hi << 4) | lo) ? 0 : -1;
}

/* ── payload parsing (checksum already validated) ──────────────────── */

typedef struct { const char *p; size_t len; } field_t;

#define NMEA_MAX_FIELDS 16

/* split body fields on commas; returns field count, or -1 if too many */
static int split_fields(const char *body, size_t body_len,
                        field_t *fields, int max)
{
    int n = 0;
    const char *p = body;
    for (size_t i = 0; i <= body_len; i++) {
        if (i == body_len || body[i] == ',') {
            if (n >= max) return -1;
            fields[n].p = p;
            fields[n].len = (size_t)(&body[i] - p);
            n++;
            p = body + i + 1;
        }
    }
    return n;
}

static int field_is(const field_t *f, const char *lit)
{
    size_t i = 0;
    for (; i < f->len; i++)
        if (f->p[i] != lit[i]) return 0;
    return lit[i] == '\0' && i == f->len;
}

static int field_int(const field_t *f, long long max, long long *out)
{
    long long v = parse_uint(f->p, f->len);
    if (v < 0 || v > max) return -1;
    *out = v;
    return 0;
}

/* parse the core five into out; 0 known, 1 unknown-ok, -1 bad fmt */
static int parse_payload(const char *line, size_t len, nmea_sentence_t *out)
{
    /* line[0]=='$', checksum is the last 3 chars "*HH" */
    size_t star = len - 3;
    if (line[star] != '*') return -1;
    const char *body = line + 1;
    size_t body_len = star - 1;

    /* address field = up to first comma; 5 chars: talker(2) + type(3) */
    size_t addr_len = 0;
    while (addr_len < body_len && body[addr_len] != ',') addr_len++;
    if (addr_len != 5) return -1;
    for (size_t i = 0; i < 5; i++)
        out->addr[i] = body[i];
    out->addr[5] = '\0';
    const char *type = body + 2;

    /* fields after the address */
    field_t f[NMEA_MAX_FIELDS];
    int nf = split_fields(body + 6, body_len - 6, f, NMEA_MAX_FIELDS);
    if (nf < 0) return -1;

    long long tmp;
    /* ── GGA: fix, sats, position ── */
    if (type[0] == 'G' && type[1] == 'G' && type[2] == 'A') {
        if (nf < 7) return -1;
        out->known = 1;
        if (field_int(&f[5], 15, &tmp) != 0) return -1;
        out->fix_quality = (int)tmp;
        out->has_fix = out->fix_quality > 0 ? 1 : 0;
        if (field_int(&f[6], 99, &tmp) != 0) return -1;
        out->n_sat = (int)tmp;
        if (out->has_fix) {
            if (f[2].len != 1 || f[4].len != 1) return -1;
            if (nmea_coord_to_udeg(f[1].p, f[1].len, f[2].p[0], 0,
                                   &out->lat_udeg) != 0) return -1;
            if (nmea_coord_to_udeg(f[3].p, f[3].len, f[4].p[0], 1,
                                   &out->lon_udeg) != 0) return -1;
        }
        return 0;
    }
    /* ── RMC: status, position, sog, cog ── */
    if (type[0] == 'R' && type[1] == 'M' && type[2] == 'C') {
        if (nf < 8) return -1;
        out->known = 1;
        out->has_fix = f[1].len == 1 && f[1].p[0] == 'A' ? 1 : 0;
        if (out->has_fix) {
            if (f[3].len != 1 || f[5].len != 1) return -1;
            if (nmea_coord_to_udeg(f[2].p, f[2].len, f[3].p[0], 0,
                                   &out->lat_udeg) != 0) return -1;
            if (nmea_coord_to_udeg(f[4].p, f[4].len, f[5].p[0], 1,
                                   &out->lon_udeg) != 0) return -1;
            if (f[6].len && nmea_fixed_to_u(f[6].p, f[6].len,
                                            &out->sog_ukn) != 0) return -1;
            if (f[7].len && nmea_fixed_to_u(f[7].p, f[7].len,
                                            &out->cog_udeg) != 0) return -1;
        }
        return 0;
    }
    /* ── HDT: true heading ── */
    if (type[0] == 'H' && type[1] == 'D' && type[2] == 'T') {
        if (nf < 2) return -1;
        out->known = 1;
        if (!field_is(&f[1], "T")) return -1;    /* HDT is TRUE heading */
        if (nmea_fixed_to_u(f[0].p, f[0].len, &out->heading_udeg) != 0)
            return -1;
        if (out->heading_udeg > 360000000) return -1;  /* [0,360] lenient */
        return 0;
    }
    /* ── DBT: depth below transducer (meters, else feet, else fathoms) ── */
    if (type[0] == 'D' && type[1] == 'B' && type[2] == 'T') {
        if (nf < 4) return -1;
        out->known = 1;
        int32_t u;
        if (f[2].len) {                          /* field 3: meters */
            if (nmea_fixed_to_u(f[2].p, f[2].len, &u) != 0) return -1;
            out->depth_um = u;
        } else if (f[0].len) {                   /* field 1: feet -> m */
            if (nmea_fixed_to_u(f[0].p, f[0].len, &u) != 0) return -1;
            out->depth_um = (int32_t)((int64_t)u * 3048 / 10000);
        } else if (nf >= 6 && f[4].len) {        /* field 5: fathoms -> m
                                                    (6 ft = 1.8288 m exact) */
            if (nmea_fixed_to_u(f[4].p, f[4].len, &u) != 0) return -1;
            out->depth_um = (int32_t)((int64_t)u * 18288 / 10000);
        } else {
            return -1;                           /* no reading at all */
        }
        return 0;
    }
    /* ── VHW: water speed (knots, else km/h) ── */
    if (type[0] == 'V' && type[1] == 'H' && type[2] == 'W') {
        if (nf < 5) return -1;
        out->known = 1;
        int32_t u;
        if (f[4].len) {                          /* field 5: knots */
            if (nmea_fixed_to_u(f[4].p, f[4].len, &u) != 0) return -1;
            out->wspeed_ukn = u;
        } else if (nf >= 8 && f[6].len) {        /* field 7: km/h -> kn */
            if (nmea_fixed_to_u(f[6].p, f[6].len, &u) != 0) return -1;
            out->wspeed_ukn = (int32_t)((int64_t)u * 5 / 18);
        } else {
            return -1;
        }
        return 0;
    }

    out->known = 0;                              /* valid, not core set */
    return 1;
}

/* ── the byte-at-a-time framer ─────────────────────────────────────── */

void nmea_reset(nmea_ctx_t *ctx)
{
    for (size_t i = 0; i < sizeof(*ctx); i++)
        ((char *)ctx)[i] = 0;                    /* counters + state zero */
}

int nmea_feed(nmea_ctx_t *ctx, char c)
{
    if (ctx->overflow) {                         /* swallowing a too-long
                                                    line until its '\n' */
        if (c == '\n') { ctx->overflow = 0; ctx->len = 0; ctx->got_star = 0; }
        return NMEA_MORE;
    }
    if (c == '\r') return NMEA_MORE;

    if (c == '$') {                              /* open / resync */
        ctx->len = 0;
        ctx->line[ctx->len++] = '$';
        ctx->xor_acc = 0;
        ctx->got_star = 0;
        ctx->ck_idx = 0;
        ctx->ck_given = 0;
        return NMEA_MORE;
    }

    if (ctx->len == 0)                           /* noise between sentences */
        return NMEA_MORE;

    if (c == '\n') {                             /* close: finalize */
        int rc;
        if (!ctx->got_star || ctx->ck_idx != 2) {
            ctx->n_bad_ck++;                     /* '*' or 'HH' missing */
            rc = NMEA_BAD_CK;
        } else if (ctx->xor_acc != ctx->ck_given) {
            ctx->n_bad_ck++;
            rc = NMEA_BAD_CK;
        } else {
            nmea_sentence_t out;
            for (size_t i = 0; i < sizeof(out); i++)
                ((char *)&out)[i] = 0;
            int pr = parse_payload(ctx->line, ctx->len, &out);
            if (pr < 0) {
                ctx->n_bad_fmt++;
                rc = NMEA_BAD_FMT;
            } else {
                ctx->out = out;
                if (pr == 0) ctx->n_ok++;
                else          ctx->n_unknown++;
                rc = NMEA_DONE;
            }
        }
        ctx->len = 0;
        ctx->got_star = 0;
        ctx->ck_idx = 0;
        return rc;
    }

    if (ctx->got_star) {
        int hv = hex_val(c);
        if (ctx->ck_idx >= 2 || hv < 0) {        /* junk after 'HH' — the
                                                    line can only fail now */
            ctx->n_bad_ck++;
            ctx->len = 0;
            ctx->got_star = 0;
            ctx->ck_idx = 0;
            return NMEA_BAD_CK;                  /* not length-related:
                                                    resync on '$' (bytes
                                                    before it are noise) */
        }
        if (ctx->len + 1 >= NMEA_MAX_LINE) {     /* checksum chars count
                                                    toward the line too */
            ctx->n_overflow++;
            ctx->overflow = 1;
            return NMEA_OVERFLOW;
        }
        ctx->ck_given = (uint8_t)((ctx->ck_given << 4) | (uint8_t)hv);
        ctx->ck_idx++;
        ctx->line[ctx->len++] = c;
        return NMEA_MORE;
    }

    if (c == '*') {
        if (ctx->len + 1 >= NMEA_MAX_LINE) {
            ctx->n_overflow++;
            ctx->overflow = 1;
            return NMEA_OVERFLOW;
        }
        ctx->got_star = 1;
        ctx->ck_idx = 0;
        ctx->line[ctx->len++] = c;
        return NMEA_MORE;
    }

    /* ordinary body byte */
    if (ctx->len + 1 >= NMEA_MAX_LINE) {
        ctx->n_overflow++;
        ctx->overflow = 1;                       /* drop the rest of line */
        return NMEA_OVERFLOW;
    }
    ctx->line[ctx->len++] = c;
    ctx->xor_acc ^= (uint8_t)c;
    return NMEA_MORE;
}
