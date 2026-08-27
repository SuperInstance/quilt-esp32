/* qm_opcodes.h — the five canon opcodes (Paper 211) as C functions on
 * firmware: BIND/LINK/EFFECT/VIEW/TICK, same names, same semantics the
 * vendored quilt-vm-c implements (qvm_bind/link/effect/view/tick). This
 * layer WRAPS the VM, it does not reimplement it — the vendored
 * vm/quilt_vm.{c,h} stays byte-identical to upstream (commit 4adf4ab), so
 * firmware code speaks the canon names only and re-vendoring stays clean.
 *
 * The ESP32 is a polyformalism of the Quilt (no. 9 if you're counting),
 * not a custom format: whatever this layer accepts, the reference VMs
 * accept, and vice versa.
 *
 * C99 + stdlib only; strdup is fine (Arduino newlib has it); no POSIX-only
 * calls, no stdio, return codes instead of fprintf/exit — same rules as
 * qm_serve.h, because qm_serve.c now routes through this layer.
 */
#ifndef QM_OPCODES_H
#define QM_OPCODES_H

#include "quilt_vm.h"

/* ── the five canon opcodes (thin pass-throughs to quilt_vm.c) ──
 *
 * BIND(name, value)      make a thing. qm_bind mirrors qvm_bind: value is
 *                        owned by the caller until install, free_value
 *                        (may be NULL) is its destructor.
 * LINK(a, b, type)       connect things; missing endpoints are BINDed
 *                        implicitly; the reverse edge is "!type".
 * EFFECT(target, fn,inv) queue a reversible change; applies when TICK
 *                        drains the pending queue; inverse runs on
 *                        dispose/free (LIFO).
 * VIEW(target, viewer)   project the thing's value for the viewer
 *                        (NULL if no such thing).
 * TICK(dt)               advance time by dt, drain pending effects,
 *                        fire due perception checks, notify subscribers.
 */
int   qm_bind(qvm_t *vm, const char *name, void *value,
              void (*free_value)(void *));
int   qm_link(qvm_t *vm, const char *a, const char *b, const char *type);
int   qm_effect(qvm_t *vm, const char *target,
                qvm_effect_fn forward, qvm_effect_fn inverse, void *arg);
void *qm_view(qvm_t *vm, const char *target, const char *viewer);
void  qm_tick(qvm_t *vm, double dt);

/* ── canon-string conveniences (the .qm world's values are canonical
 *    JSON strings; these are what qm_serve.c uses — wrap, not duplicate) ── */

/* BIND with a canon-JSON string value: strdups value (NULL = primitive
 * NULL value), installs free() as the destructor. Returns 0 ok, -1 error. */
int qm_bind_str(qvm_t *vm, const char *name, const char *canon);

/* EFFECT(QM_SET): queue "install canon string into target" with inverse
 * "set NULL". Applies on the next qm_tick. The strdup is owned by the
 * thing after apply. Returns 0 ok, -1 alloc/unknown-target error. */
int qm_effect_set(qvm_t *vm, const char *target, const char *canon);

/* shared destructor for canon-string values (free()). */
void qm_value_free(void *p);

#endif /* QM_OPCODES_H */
