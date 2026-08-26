/* Vendored verbatim from SuperInstance/quilt-vm-c, src/quilt_vm.h
 * (commit 4adf4ab67f6c5ffd1e9ce81e1ef851454c59788f) — vendored 2026-08-26.
 * Everything below this comment is unmodified upstream code. */
/* quilt_vm.h — The 5-opcode Quilt VM in C.
 *
 * A runtime is a function from context to value with an inverse,
 * advanced by a clock that processes async I/O while projecting
 * a sync view.
 *
 * The 5 opcodes:
 *   BIND(name, value)    -- make a thing
 *   LINK(a, b, type)     -- connect things
 *   EFFECT(target, fn, inv) -- reversible change
 *   VIEW(target, viewer) -- project for viewer
 *   TICK(dt)             -- advance time, drain I/O
 *
 * The 8 polyformalisms:
 *   1. Quilt cell
 *   2. Cordis plugin
 *   3. Spreadsheet
 *   4. MUD
 *   5. TTRPG (with perception check)
 *   6. The bay dance
 *   7. The cowboy
 *   8. The bus
 */
#ifndef QUILT_VM_H
#define QUILT_VM_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
typedef struct qvm_thing qvm_thing_t;
typedef struct qvm_event qvm_event_t;
typedef struct qvm qvm_t;
typedef void (*qvm_effect_fn)(qvm_thing_t *target, void *arg);
typedef void *(*qvm_view_fn)(qvm_thing_t *target, const char *viewer, void *arg);
typedef void (*qvm_subscriber_fn)(qvm_event_t *event, void *arg);
typedef void (*qvm_scheduled_fn)(qvm_t *vm, void *arg);

/* A thing in the VM. The unit of composition. */
struct qvm_thing {
    char *name;            /* The address (spatial coordinate) */
    void *value;           /* The data */
    void (*free_value)(void *);  /* Destructor for value (NULL for primitive) */

    /* Links (topology) */
    char **link_types;     /* Array of link type names */
    char ***link_targets;   /* Array of arrays of target names per type */
    size_t n_link_types;

    /* Effects (reversible) */
    struct qvm_effect_record *effects;
    size_t n_effects;
    size_t effects_capacity;
};

struct qvm_effect_record {
    char *target;
    qvm_effect_fn forward;
    qvm_effect_fn inverse;
    void *arg;
};

struct qvm_event {
    double ts;
    char kind[32];
    char target[64];
    void *old_value;
    void *new_value;
};

/* A scheduled perception check */
typedef struct {
    char *key;
    qvm_scheduled_fn fn;
    void *arg;
    double at;
} qvm_scheduled_t;

/* The VM. The 5-opcode runtime. */
struct qvm {
    qvm_thing_t **things;     /* Array of things, indexed by hash */
    size_t n_things;
    size_t things_capacity;

    double time;

    /* Pending effects (async I/O queue) */
    struct qvm_effect_record *pending;
    size_t n_pending;
    size_t pending_capacity;

    /* Event log */
    qvm_event_t *event_log;
    size_t n_events;
    size_t events_capacity;

    /* Subscribers (the bus) */
    qvm_subscriber_fn *subscribers;
    void **subscriber_args;
    size_t n_subscribers;
    size_t subscribers_capacity;

    /* Scheduled perception checks */
    qvm_scheduled_t *scheduled;
    size_t n_scheduled;
    size_t scheduled_capacity;
};

/* --- Constructor / Destructor --- */

qvm_t *qvm_new(void);
void qvm_free(qvm_t *vm);

/* --- Opcode 1: BIND --- */
int qvm_bind(qvm_t *vm, const char *name, void *value,
              void (*free_value)(void *));

/* --- Opcode 2: LINK --- */
int qvm_link(qvm_t *vm, const char *a, const char *b, const char *type);

/* --- Opcode 3: EFFECT --- */
int qvm_effect(qvm_t *vm, const char *target,
                qvm_effect_fn forward, qvm_effect_fn inverse,
                void *arg);

/* --- Opcode 4: VIEW --- */
void *qvm_view(qvm_t *vm, const char *target, const char *viewer);

/* --- Opcode 5: TICK --- */
void qvm_tick(qvm_t *vm, double dt);

/* --- Dispose (run effects in REVERSE) --- */
void qvm_dispose(qvm_t *vm, const char *target);

/* --- Schedule (perception check) --- */
int qvm_schedule(qvm_t *vm, const char *key, qvm_scheduled_fn fn,
                  void *arg, double at);

/* --- Subscribe (the bus) --- */
int qvm_subscribe(qvm_t *vm, qvm_subscriber_fn fn, void *arg);

/* --- Stats --- */
void qvm_stats(qvm_t *vm, char *out, size_t out_size);

/* --- Find a thing by name (used by view_fn) --- */
qvm_thing_t *qvm_find(qvm_t *vm, const char *name);

/* --- Get/Set value (for effects) --- */
void *qvm_thing_get(qvm_thing_t *thing);
void qvm_thing_set(qvm_thing_t *thing, void *value, void (*free_value)(void *));

#endif /* QUILT_VM_H */
