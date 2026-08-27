/* host_opcodes/main.c — host unit tests for qm_opcodes (the five canon
 * opcodes, Paper 211: BIND/LINK/EFFECT/VIEW/TICK) over the vendored
 * quilt-vm-c, plus a serve-path regression proving the refactored qm_serve
 * (which now routes through this layer) still answers the fixture exactly
 * as the Pass A/B equivalence run did. One JSON summary line at the end;
 * exit 0 iff all checks pass. */
#include <stdio.h>
#include <string.h>
#include "qm_serve.h"

static int passed = 0, failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { passed++; } \
    else { failed++; printf("{\"fail\":\"%s\"}\n", name); } \
} while (0)

static int streq(const void *v, const char *s) {
    return v != NULL && strcmp((const char *)v, s) == 0;
}

/* canon payload value for key "phase" (already quoted by canonicalization) */
static const char *signal_phase(const QmSignal *s) {
    for (int j = 0; j < s->n_payload; j++)
        if (strcmp(s->payload[j].key, "phase") == 0)
            return s->payload[j].canon;
    return NULL;
}

int main(void) {
    /* ── BIND ── */
    qvm_t *vm = qvm_new();
    CHECK(vm != NULL, "qvm_new");
    CHECK(qm_bind_str(vm, "t1", "{\"led\":true}") == 0, "qm_bind_str rc");
    CHECK(streq(qm_view(vm, "t1", "anyone"), "{\"led\":true}"),
          "BIND: view returns canon value");
    CHECK(qm_bind(vm, "t2", NULL, NULL) == 0, "qm_bind raw rc");
    CHECK(qvm_find(vm, "t2") != NULL && qm_view(vm, "t2", "x") == NULL,
          "BIND: raw NULL value, thing exists, view NULL");
    CHECK(qm_view(vm, "no-such-thing", "anyone") == NULL,
          "VIEW: unknown target -> NULL");

    /* ── LINK ── */
    CHECK(qm_link(vm, "a", "b", "feeds") == 0, "qm_link rc");
    CHECK(qvm_find(vm, "a") != NULL && qvm_find(vm, "b") != NULL,
          "LINK: missing endpoints implicitly BINDed");
    {
        qvm_thing_t *ta = qvm_find(vm, "a"), *tb = qvm_find(vm, "b");
        CHECK(ta->n_link_types == 1 && strcmp(ta->link_types[0], "feeds") == 0
              && strcmp(ta->link_targets[0][0], "b") == 0,
              "LINK: forward edge a -feeds-> b");
        CHECK(tb->n_link_types == 1 && strcmp(tb->link_types[0], "!feeds") == 0
              && strcmp(tb->link_targets[0][0], "a") == 0,
              "LINK: reverse edge b -!feeds-> a");
    }
    CHECK(qm_link(vm, "a", "c", "feeds") == 0, "qm_link 2nd edge rc");
    {
        qvm_thing_t *ta = qvm_find(vm, "a");
        CHECK(ta->n_link_types == 1
              && strcmp(ta->link_targets[0][0], "b") == 0
              && strcmp(ta->link_targets[0][1], "c") == 0
              && ta->link_targets[0][2] == NULL,
              "LINK: second edge of same type appends, no new type");
    }

    /* ── EFFECT + TICK ── */
    CHECK(qm_effect_set(vm, "t1", "{\"led\":false}") == 0, "qm_effect_set rc");
    CHECK(streq(qm_view(vm, "t1", "anyone"), "{\"led\":true}"),
          "EFFECT: queued, not applied before TICK");
    CHECK(qm_effect(vm, "no-such-thing", NULL, NULL, NULL) == -1,
          "EFFECT: unknown target -> -1");
    qm_tick(vm, 2.5);
    CHECK(streq(qm_view(vm, "t1", "anyone"), "{\"led\":false}"),
          "EFFECT+TICK: pending drained, new value visible");
    CHECK(vm->n_pending == 0, "TICK: pending queue empty after drain");
    CHECK(vm->time == 2.5, "TICK: time advanced by dt");
    CHECK(vm->n_events >= 1, "TICK: effect application logged");
    qvm_dispose(vm, "t1");
    CHECK(qm_view(vm, "t1", "anyone") == NULL,
          "dispose: inverse ran, value back to NULL");
    qvm_free(vm);

    /* ── the five, end to end, by hand (no qm_serve): a two-rule world ── */
    vm = qvm_new();
    CHECK(qm_bind_str(vm, "lamp:response", NULL) == 0
          && qm_link(vm, "switch", "lamp", "controls") == 0
          && qm_effect_set(vm, "lamp:response", "{\"on\":true}") == 0,
          "canon: bind+link+effect queue");
    qm_tick(vm, 1.0);
    CHECK(streq(qm_view(vm, "lamp:response", "anyone"), "{\"on\":true}"),
          "canon: tick applies, view reads back");
    qvm_free(vm);

    /* ── serve regression: qm_serve through the canon layer == Pass A/B ── */
    CHECK(qm_serve_init(&vm) == 0, "qm_serve_init");
    {
        static const char *want_mode[5] =
            { "table", "table", "table-miss", "table", "table" };
        static const int want_led[5] = { 1, 0, -1, 1, 0 };
        int all = 1;
        for (int i = 0; i < QM_N_SIGNALS; i++) {
            const QmSignal *s = &qm_signals[i];
            char mode[16];
            const char *resp = NULL;
            int rc = qm_serve(vm, s, mode, &resp);
            int led = qm_led_from_response(resp);
            if (rc != 0 || strcmp(mode, want_mode[i]) != 0
                || led != want_led[i]) all = 0;
            if (want_led[i] == 1 && !streq(resp, "{\"led\":true}")) all = 0;
            if (want_led[i] == 0 && !streq(resp, "{\"led\":false}")) all = 0;
            if (want_led[i] == -1 && resp != NULL) all = 0;
            const char *phase = signal_phase(s);
            printf("{\"i\":%d,\"kind\":\"%s\",\"phase\":%s,\"mode\":\"%s\","
                   "\"response\":%s,\"led\":%d}\n",
                   i, s->kind, phase ? phase : "null",
                   rc == 0 ? mode : "serve-error",
                   resp ? resp : "null", led);
        }
        CHECK(all, "serve regression: 5/5 signals match Pass A/B output");
    }
    qm_serve_free(vm);

    printf("{\"ok\":%s,\"passed\":%d,\"failed\":%d}\n",
           failed == 0 ? "true" : "false", passed, failed);
    return failed == 0 ? 0 : 1;
}
