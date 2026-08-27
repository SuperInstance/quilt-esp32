/* qm_serve.h — portable serve path for compiled .qm rule tables (blink limb).
 * C99 + stdlib only (strdup is fine — Arduino newlib has it); no POSIX-only
 * calls, no stdio in the serve path, error codes instead of fprintf/exit.
 * Semantics adapted from qm_bench.c (SuperInstance/cell-cascade, branch
 * edge-benchmarks) onto the real quilt-vm-c 5-opcode VM. */
#ifndef QM_SERVE_H
#define QM_SERVE_H

#include "quilt_vm.h"
#include "qm_tables.h"
#include "qm_opcodes.h"

/* qm_opcodes.h: the five canon opcodes (Paper 211) as C — qm_bind,
 * qm_link, qm_effect, qm_view, qm_tick — wrapping the vendored VM.
 * qm_serve.c routes through them; this header re-exports the surface. */

/* qvm_new + BIND every bind + LINK every link. Returns 0 ok, -1 error. */
int qm_serve_init(qvm_t **vm_out);

/* One serve on the real VM: route to "<to>:response", first matching guard
 * wins (kind equality + canonical-JSON payload_equals subset by strcmp);
 * hit  -> strdup(set_canon) + qvm_effect(QM_SET) + qvm_tick(1.0), response
 *         read back via qvm_view, mode "table";
 * miss -> effect installs {"miss":true} inside the VM, qvm_tick(1.0),
 *         *response_out = NULL, mode "table-miss".
 * Returns 0 ok, -1 VM/alloc error, -2 rule with QM_EXPR action (blink is
 * QM_SET only). The returned response points at VM-owned memory valid until
 * the next serve of the same target. */
int qm_serve(qvm_t *vm, const QmSignal *s, char mode_out[16], const char **response_out);

/* 1 if response canon contains "led":true, 0 if "led":false, -1 if NULL
 * (or neither marker present). Canonical JSON has no spaces, and the set
 * object is exactly {"led":true} / {"led":false}, so substring match is
 * exact. */
int qm_led_from_response(const char *response);

/* qvm_free. */
void qm_serve_free(qvm_t *vm);

#endif /* QM_SERVE_H */
