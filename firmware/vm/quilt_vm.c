/* Vendored verbatim from SuperInstance/quilt-vm-c, src/quilt_vm.c
 * (commit 4adf4ab67f6c5ffd1e9ce81e1ef851454c59788f) — vendored 2026-08-26.
 * Everything below this comment is unmodified upstream code. */
/* quilt_vm.c — Implementation of the 5-opcode Quilt VM. */

#include "quilt_vm.h"

/* === Thing === */

qvm_thing_t *qvm_thing_new(const char *name, void *value,
                              void (*free_value)(void *)) {
    qvm_thing_t *t = (qvm_thing_t *)calloc(1, sizeof(qvm_thing_t));
    if (!t) return NULL;
    t->name = strdup(name);
    t->value = value;
    t->free_value = free_value;
    t->effects_capacity = 4;
    t->effects = (struct qvm_effect_record *)
        calloc(t->effects_capacity, sizeof(struct qvm_effect_record));
    return t;
}

void qvm_thing_free(qvm_thing_t *t) {
    if (!t) return;
    /* Run all effects in REVERSE order (LIFO) */
    for (size_t i = t->n_effects; i > 0; i--) {
        if (t->effects[i-1].inverse) {
            t->effects[i-1].inverse(t, t->effects[i-1].arg);
        }
    }
    free(t->effects);
    for (size_t i = 0; i < t->n_link_types; i++) {
        free(t->link_types[i]);
        for (size_t j = 0; t->link_targets[i][j]; j++) {
            free(t->link_targets[i][j]);
        }
        free(t->link_targets[i]);
    }
    free(t->link_types);
    free(t->link_targets);
    if (t->free_value && t->value) {
        t->free_value(t->value);
    }
    free(t->name);
    free(t);
}

void *qvm_thing_get(qvm_thing_t *thing) { return thing ? thing->value : NULL; }
void qvm_thing_set(qvm_thing_t *thing, void *value,
                    void (*free_value)(void *)) {
    if (thing->free_value && thing->value && thing->value != value) {
        thing->free_value(thing->value);
    }
    thing->value = value;
    thing->free_value = free_value;
}

/* === VM === */

qvm_t *qvm_new(void) {
    qvm_t *vm = (qvm_t *)calloc(1, sizeof(qvm_t));
    if (!vm) return NULL;
    vm->things_capacity = 16;
    vm->things = (qvm_thing_t **)calloc(vm->things_capacity,
                                          sizeof(qvm_thing_t *));
    vm->pending_capacity = 16;
    vm->pending = (struct qvm_effect_record *)
        calloc(vm->pending_capacity, sizeof(struct qvm_effect_record));
    vm->events_capacity = 256;
    vm->event_log = (qvm_event_t *)calloc(vm->events_capacity,
                                            sizeof(qvm_event_t));
    vm->subscribers_capacity = 4;
    vm->subscribers = (qvm_subscriber_fn *)calloc(vm->subscribers_capacity,
                                                   sizeof(qvm_subscriber_fn));
    vm->subscriber_args = (void **)calloc(vm->subscribers_capacity,
                                            sizeof(void *));
    vm->scheduled_capacity = 16;
    vm->scheduled = (qvm_scheduled_t *)calloc(vm->scheduled_capacity,
                                                sizeof(qvm_scheduled_t));
    return vm;
}

void qvm_free(qvm_t *vm) {
    if (!vm) return;
    for (size_t i = 0; i < vm->n_things; i++) {
        qvm_thing_free(vm->things[i]);
    }
    free(vm->things);
    for (size_t i = 0; i < vm->n_pending; i++) {
        free(vm->pending[i].target);
    }
    free(vm->pending);
    free(vm->event_log);
    free(vm->subscribers);
    free(vm->subscriber_args);
    for (size_t i = 0; i < vm->n_scheduled; i++) {
        free(vm->scheduled[i].key);
    }
    free(vm->scheduled);
    free(vm);
}

qvm_thing_t *qvm_find(qvm_t *vm, const char *name) {
    for (size_t i = 0; i < vm->n_things; i++) {
        if (strcmp(vm->things[i]->name, name) == 0) {
            return vm->things[i];
        }
    }
    return NULL;
}

/* Opcode 1: BIND */
int qvm_bind(qvm_t *vm, const char *name, void *value,
              void (*free_value)(void *)) {
    if (vm->n_things >= vm->things_capacity) {
        vm->things_capacity *= 2;
        vm->things = (qvm_thing_t **)realloc(vm->things,
            vm->things_capacity * sizeof(qvm_thing_t *));
    }
    qvm_thing_t *t = qvm_thing_new(name, value, free_value);
    if (!t) return -1;
    vm->things[vm->n_things++] = t;
    return 0;
}

/* Opcode 2: LINK */
int qvm_link(qvm_t *vm, const char *a, const char *b, const char *type) {
    qvm_thing_t *ta = qvm_find(vm, a);
    qvm_thing_t *tb = qvm_find(vm, b);
    if (!ta) {
        qvm_bind(vm, a, NULL, NULL);
        ta = qvm_find(vm, a);
    }
    if (!tb) {
        qvm_bind(vm, b, NULL, NULL);
        tb = qvm_find(vm, b);
    }
    /* Add a -> b of type */
    for (size_t i = 0; i < ta->n_link_types; i++) {
        if (strcmp(ta->link_types[i], type) == 0) {
            /* Append to existing */
            size_t n = 0;
            while (ta->link_targets[i][n]) n++;
            ta->link_targets[i] = (char **)realloc(ta->link_targets[i],
                (n + 2) * sizeof(char *));
            ta->link_targets[i][n] = strdup(b);
            ta->link_targets[i][n+1] = NULL;
            goto reverse_link;
        }
    }
    /* New type for a */
    ta->link_types = (char **)realloc(ta->link_types,
        (ta->n_link_types + 2) * sizeof(char *));
    ta->link_targets = (char ***)realloc(ta->link_targets,
        (ta->n_link_types + 2) * sizeof(char **));
    ta->link_types[ta->n_link_types] = strdup(type);
    ta->link_targets[ta->n_link_types] = (char **)calloc(2, sizeof(char *));
    ta->link_targets[ta->n_link_types][0] = strdup(b);
    ta->link_targets[ta->n_link_types][1] = NULL;
    ta->n_link_types++;

reverse_link: {
    char reverse_type[128];
    snprintf(reverse_type, sizeof(reverse_type), "!%s", type);
    for (size_t i = 0; i < tb->n_link_types; i++) {
        if (strcmp(tb->link_types[i], reverse_type) == 0) {
            size_t n = 0;
            while (tb->link_targets[i][n]) n++;
            tb->link_targets[i] = (char **)realloc(tb->link_targets[i],
                (n + 2) * sizeof(char *));
            tb->link_targets[i][n] = strdup(a);
            tb->link_targets[i][n+1] = NULL;
            return 0;
        }
    }
    tb->link_types = (char **)realloc(tb->link_types,
        (tb->n_link_types + 2) * sizeof(char *));
    tb->link_targets = (char ***)realloc(tb->link_targets,
        (tb->n_link_types + 2) * sizeof(char **));
    tb->link_types[tb->n_link_types] = strdup(reverse_type);
    tb->link_targets[tb->n_link_types] = (char **)calloc(2, sizeof(char *));
    tb->link_targets[tb->n_link_types][0] = strdup(a);
    tb->link_targets[tb->n_link_types][1] = NULL;
    tb->n_link_types++;
    }
    return 0;
}

/* Opcode 3: EFFECT */
int qvm_effect(qvm_t *vm, const char *target,
                qvm_effect_fn forward, qvm_effect_fn inverse, void *arg) {
    qvm_thing_t *t = qvm_find(vm, target);
    if (!t) return -1;
    /* Queue for async processing */
    if (vm->n_pending >= vm->pending_capacity) {
        vm->pending_capacity *= 2;
        vm->pending = (struct qvm_effect_record *)realloc(vm->pending,
            vm->pending_capacity * sizeof(struct qvm_effect_record));
    }
    struct qvm_effect_record *e = &vm->pending[vm->n_pending++];
    e->target = strdup(target);
    e->forward = forward;
    e->inverse = inverse;
    e->arg = arg;
    /* Also add to thing's effect list (for dispose) */
    if (t->n_effects >= t->effects_capacity) {
        t->effects_capacity *= 2;
        t->effects = (struct qvm_effect_record *)realloc(t->effects,
            t->effects_capacity * sizeof(struct qvm_effect_record));
    }
    t->effects[t->n_effects].target = strdup(target);
    t->effects[t->n_effects].forward = forward;
    t->effects[t->n_effects].inverse = inverse;
    t->effects[t->n_effects].arg = arg;
    t->n_effects++;
    return 0;
}

/* Opcode 4: VIEW */
void *qvm_view(qvm_t *vm, const char *target, const char *viewer) {
    (void)viewer;  /* Unused for now; the projection hook could use it */
    qvm_thing_t *t = qvm_find(vm, target);
    return t ? t->value : NULL;
}

/* Opcode 5: TICK */
void qvm_tick(qvm_t *vm, double dt) {
    vm->time += dt;
    /* Process pending effects */
    size_t n = vm->n_pending;
    for (size_t i = 0; i < n; i++) {
        struct qvm_effect_record e = vm->pending[i];
        qvm_thing_t *t = qvm_find(vm, e.target);
        if (t && e.forward) {
            e.forward(t, e.arg);
        }
        /* Log event */
        if (vm->n_events >= vm->events_capacity) {
            vm->events_capacity *= 2;
            vm->event_log = (qvm_event_t *)realloc(vm->event_log,
                vm->events_capacity * sizeof(qvm_event_t));
        }
        qvm_event_t *ev = &vm->event_log[vm->n_events++];
        ev->ts = vm->time;
        strncpy(ev->kind, "effect.applied", sizeof(ev->kind) - 1);
        strncpy(ev->target, e.target, sizeof(ev->target) - 1);
        ev->old_value = NULL;
        ev->new_value = NULL;
        free(e.target);
    }
    vm->n_pending = 0;
    /* Fire scheduled perception checks */
    for (size_t i = 0; i < vm->n_scheduled; i++) {
        if (vm->scheduled[i].at <= vm->time) {
            qvm_scheduled_fn fn = vm->scheduled[i].fn;
            void *arg = vm->scheduled[i].arg;
            /* Remove this entry */
            free(vm->scheduled[i].key);
            for (size_t j = i; j < vm->n_scheduled - 1; j++) {
                vm->scheduled[j] = vm->scheduled[j + 1];
            }
            vm->n_scheduled--;
            i--;
            if (fn) fn(vm, arg);
        }
    }
    /* Notify subscribers */
    qvm_event_t tick_event;
    tick_event.ts = vm->time;
    strncpy(tick_event.kind, "tick", sizeof(tick_event.kind) - 1);
    tick_event.target[0] = '\0';
    tick_event.old_value = NULL;
    tick_event.new_value = NULL;
    for (size_t i = 0; i < vm->n_subscribers; i++) {
        if (vm->subscribers[i]) {
            vm->subscribers[i](&tick_event, vm->subscriber_args[i]);
        }
    }
}

/* Dispose */
void qvm_dispose(qvm_t *vm, const char *target) {
    qvm_thing_t *t = qvm_find(vm, target);
    if (!t) return;
    /* Run all effects in REVERSE order (LIFO) */
    for (size_t i = t->n_effects; i > 0; i--) {
        if (t->effects[i-1].inverse) {
            t->effects[i-1].inverse(t, t->effects[i-1].arg);
        }
    }
    t->n_effects = 0;
    if (t->free_value && t->value) {
        t->free_value(t->value);
    }
    t->value = NULL;
}

/* Schedule */
int qvm_schedule(qvm_t *vm, const char *key, qvm_scheduled_fn fn,
                  void *arg, double at) {
    if (vm->n_scheduled >= vm->scheduled_capacity) {
        vm->scheduled_capacity *= 2;
        vm->scheduled = (qvm_scheduled_t *)realloc(vm->scheduled,
            vm->scheduled_capacity * sizeof(qvm_scheduled_t));
    }
    qvm_scheduled_t *s = &vm->scheduled[vm->n_scheduled++];
    s->key = strdup(key);
    s->fn = fn;
    s->arg = arg;
    s->at = at;
    return 0;
}

/* Subscribe */
int qvm_subscribe(qvm_t *vm, qvm_subscriber_fn fn, void *arg) {
    if (vm->n_subscribers >= vm->subscribers_capacity) {
        vm->subscribers_capacity *= 2;
        vm->subscribers = (qvm_subscriber_fn *)realloc(vm->subscribers,
            vm->subscribers_capacity * sizeof(qvm_subscriber_fn));
        vm->subscriber_args = (void **)realloc(vm->subscriber_args,
            vm->subscribers_capacity * sizeof(void *));
    }
    vm->subscribers[vm->n_subscribers] = fn;
    vm->subscriber_args[vm->n_subscribers] = arg;
    vm->n_subscribers++;
    return 0;
}

/* Stats */
void qvm_stats(qvm_t *vm, char *out, size_t out_size) {
    snprintf(out, out_size,
        "{n_things: %zu, time: %.2f, n_pending: %zu, n_events: %zu, n_scheduled: %zu, n_subscribers: %zu}",
        vm->n_things, vm->time, vm->n_pending, vm->n_events,
        vm->n_scheduled, vm->n_subscribers);
}
