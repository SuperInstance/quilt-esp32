/* qm_serve.c — portable serve path (host harness today, xtensa later).
 * Adapted from qm_bench.c (SuperInstance/cell-cascade, branch
 * edge-benchmarks): rule scan via rule_matches, hit -> QM_SET effect +
 * qvm_tick on the real quilt-vm-c, read back via qvm_view; miss ->
 * {"miss":true} in-VM with a null reported response. The QM_EXPR /
 * sigma_distance branch is dropped (blink is QM_SET only) — a rule with a
 * QM_EXPR action is an error here (-2). No fprintf/exit: return codes only. */
#include "qm_serve.h"

static void free_str(void *p) { free(p); }

/* effect forward: install the precomputed result string into the thing */
typedef struct { char *value; } SetArg;
static void fwd_set(qvm_thing_t *t, void *arg) {
    SetArg *a = (SetArg *)arg;
    qvm_thing_set(t, a->value, free_str);
}
static void inv_set(qvm_thing_t *t, void *arg) {
    (void)arg;
    qvm_thing_set(t, NULL, NULL);
}

/* guard match: kind equality + payload_equals subset by canonical strcmp */
static int rule_matches(const QmRule *r, const QmSignal *s) {
    size_t rlen = strlen(r->target), alen = strlen(s->to);
    /* target "<to>:response" */
    if (rlen != alen + 9) return 0;
    if (strncmp(r->target, s->to, alen) != 0) return 0;
    if (strcmp(r->target + alen, ":response") != 0) return 0;
    if (r->kind && strcmp(r->kind, s->kind) != 0) return 0;
    for (int i = 0; i < r->n_pe; i++) {
        int found = 0;
        for (int j = 0; j < s->n_payload; j++) {
            if (strcmp(r->pe[i].key, s->payload[j].key) == 0 &&
                strcmp(r->pe[i].canon, s->payload[j].canon) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

int qm_serve_init(qvm_t **vm_out) {
    if (!vm_out) return -1;
    qvm_t *vm = qvm_new();
    if (!vm) return -1;
    for (int i = 0; i < QM_N_BINDS; i++) {
        if (qvm_bind(vm, qm_binds[i].target,
                     qm_binds[i].canon ? strdup(qm_binds[i].canon) : NULL,
                     free_str) != 0) {
            qvm_free(vm);
            return -1;
        }
    }
    for (int i = 0; i < QM_N_LINKS; i++) {
        if (qvm_link(vm, qm_links[i].from, qm_links[i].to,
                     qm_links[i].type) != 0) {
            qvm_free(vm);
            return -1;
        }
    }
    *vm_out = vm;
    return 0;
}

int qm_serve(qvm_t *vm, const QmSignal *s, char mode_out[16], const char **response_out) {
    const QmRule *hit = NULL;
    for (int i = 0; i < QM_N_RULES; i++) {
        if (rule_matches(&qm_rules[i], s)) { hit = &qm_rules[i]; break; }
    }
    if (hit && hit->action == QM_EXPR) return -2; /* blink is QM_SET only */

    char target[256];
    snprintf(target, sizeof target, "%s:response", s->to);

    SetArg arg;
    if (hit) {
        strcpy(mode_out, "table");
        arg.value = strdup(hit->set_canon);
    } else {
        strcpy(mode_out, "table-miss");
        arg.value = strdup("{\"miss\":true}");
    }
    if (!arg.value) return -1;
    /* EFFECT + TICK on the real VM — the pending-effects drain applies it */
    if (qvm_effect(vm, target, fwd_set, inv_set, &arg) != 0) {
        free(arg.value);
        return -1;
    }
    qvm_tick(vm, 1.0);
    /* table-miss reports null (the {miss:true} stays inside the VM) */
    *response_out = hit ? (const char *)qvm_view(vm, target, "anyone") : NULL;
    return 0;
}

int qm_led_from_response(const char *response) {
    if (!response) return -1;
    if (strstr(response, "\"led\":true") != NULL) return 1;
    if (strstr(response, "\"led\":false") != NULL) return 0;
    return -1;
}

void qm_serve_free(qvm_t *vm) { qvm_free(vm); }
