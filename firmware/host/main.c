/* host/main.c — host harness: build the VM from the compiled tables, serve
 * every signal in the fixture, print one JSON line per signal. No timing
 * code. Ends with {"ok":true} if every serve returned 0. */
#include <stdio.h>
#include <string.h>
#include "qm_serve.h"

/* canon payload value for key "phase" (already quoted by canonicalization),
 * or NULL if the signal carries no phase */
static const char *signal_phase(const QmSignal *s) {
    for (int j = 0; j < s->n_payload; j++)
        if (strcmp(s->payload[j].key, "phase") == 0)
            return s->payload[j].canon;
    return NULL;
}

int main(void) {
    qvm_t *vm = NULL;
    if (qm_serve_init(&vm) != 0) {
        printf("{\"ok\":false}\n");
        return 1;
    }

    int ok = 1;
    for (int i = 0; i < QM_N_SIGNALS; i++) {
        const QmSignal *s = &qm_signals[i];
        char mode[16];
        const char *resp = NULL;
        int rc = qm_serve(vm, s, mode, &resp);
        if (rc != 0) ok = 0;
        const char *phase = signal_phase(s);
        printf("{\"i\":%d,\"kind\":\"%s\",\"phase\":%s,\"mode\":\"%s\","
               "\"response\":%s,\"led\":%d}\n",
               i, s->kind,
               phase ? phase : "null",
               rc == 0 ? mode : "serve-error",
               resp ? resp : "null",
               qm_led_from_response(resp));
    }

    qm_serve_free(vm);
    printf(ok ? "{\"ok\":true}\n" : "{\"ok\":false}\n");
    return ok ? 0 : 1;
}
