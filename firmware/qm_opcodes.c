/* qm_opcodes.c — the five canon opcodes as C, wrapping the vendored
 * quilt-vm-c one-for-one. The VM below is unmodified upstream code; this
 * file is the whole firmware-side canon layer. The set-effect plumbing
 * (fwd_set/inv_set/free_str) moved here from qm_serve.c unchanged —
 * qm_serve now calls qm_bind_str/qm_link/qm_effect_set/qm_tick/qm_view,
 * so what blinks the LED on metal IS the canon opcode set. */
#include "qm_opcodes.h" /* pulls quilt_vm.h: _POSIX_C_SOURCE before stdlib */
#include <stdlib.h>
#include <string.h>

/* ── the five canon opcodes: pure pass-throughs ── */

int qm_bind(qvm_t *vm, const char *name, void *value,
            void (*free_value)(void *)) {
    return qvm_bind(vm, name, value, free_value);
}

int qm_link(qvm_t *vm, const char *a, const char *b, const char *type) {
    return qvm_link(vm, a, b, type);
}

int qm_effect(qvm_t *vm, const char *target,
              qvm_effect_fn forward, qvm_effect_fn inverse, void *arg) {
    return qvm_effect(vm, target, forward, inverse, arg);
}

void *qm_view(qvm_t *vm, const char *target, const char *viewer) {
    return qvm_view(vm, target, viewer);
}

void qm_tick(qvm_t *vm, double dt) {
    qvm_tick(vm, dt);
}

/* ── canon-string conveniences (moved from qm_serve.c, logic unaltered) ── */

void qm_value_free(void *p) { free(p); }

int qm_bind_str(qvm_t *vm, const char *name, const char *canon) {
    return qm_bind(vm, name,
                   canon ? strdup(canon) : NULL, qm_value_free);
}

/* effect forward: install the canon string into the thing. The arg IS
 * the strdup'd canon (heap, owned by the thing after apply) — no wrapper
 * struct: the original qm_serve passed a stack SetArg and got away with
 * it only because its qm_tick ran in the same frame; hoisting the effect
 * into this wrapper exposed that lifetime (host ASan: stack-use-after-
 * return in fwd_set). Arg-is-the-value has no frame to outlive. */
static void fwd_set(qvm_thing_t *t, void *arg) {
    qvm_thing_set(t, arg, qm_value_free);
}
static void inv_set(qvm_thing_t *t, void *arg) {
    (void)arg;
    qvm_thing_set(t, NULL, NULL);
}

int qm_effect_set(qvm_t *vm, const char *target, const char *canon) {
    char *value = strdup(canon);
    if (!value) return -1;
    if (qm_effect(vm, target, fwd_set, inv_set, value) != 0) {
        free(value);
        return -1;
    }
    return 0;
}
