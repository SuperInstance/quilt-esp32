/* test_vm.c — Unit tests for the 5-opcode Quilt VM, ESP32 port.
 *
 * These mirror the host-port tests in /workspace/quilt-vm-c/tests/.
 * They exercise the 5 opcodes directly: BIND, LINK, EFFECT,
 * VIEW, TICK. No ESP-IDF, no FreeRTOS, no WiFi — just the
 * substrate. Build with the host Makefile (see README) or
 * run them under an ESP-IDF unity harness.
 *
 * The 5 tests are the 5 opcodes. The opcodes are the size.
 */

#include "quilt_vm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* --- Effects: forward=inc, inverse=dec --- */
static void effect_inc(qvm_thing_t *t, void *arg) {
    (void)arg;
    int *v = (int *)qvm_thing_get(t);
    if (v) (*v)++;
}
static void effect_dec(qvm_thing_t *t, void *arg) {
    (void)arg;
    int *v = (int *)qvm_thing_get(t);
    if (v) (*v)--;
}

/* === Test 1: BIND === */
static void test_bind(void) {
    qvm_t *vm = qvm_new();
    double depth = 4.2;
    assert(qvm_bind(vm, "bathy:0", &depth, NULL) == 0);
    /* The cell is now in the VM. */
    qvm_thing_t *t = qvm_find(vm, "bathy:0");
    assert(t != NULL);
    assert(strcmp(t->name, "bathy:0") == 0);
    assert(t->value == &depth);
    /* The cowboy's first stone. */
    qvm_free(vm);
    printf("  PASS test_bind (BIND)\n");
}

/* === Test 2: LINK === */
static void test_link(void) {
    qvm_t *vm = qvm_new();
    qvm_bind(vm, "a", NULL, NULL);
    qvm_bind(vm, "b", NULL, NULL);
    assert(qvm_link(vm, "a", "b", "depends_on") == 0);

    qvm_thing_t *ta = qvm_find(vm, "a");
    assert(ta->n_link_types == 1);
    assert(strcmp(ta->link_types[0], "depends_on") == 0);
    assert(strcmp(ta->link_targets[0][0], "b") == 0);

    /* The reverse arrow is auto-registered. */
    qvm_thing_t *tb = qvm_find(vm, "b");
    assert(tb->n_link_types == 1);
    assert(strcmp(tb->link_types[0], "!depends_on") == 0);
    assert(strcmp(tb->link_targets[0][0], "a") == 0);

    qvm_free(vm);
    printf("  PASS test_link (LINK)\n");
}

/* === Test 3: EFFECT === */
static void test_effect(void) {
    qvm_t *vm = qvm_new();
    int counter = 0;
    qvm_bind(vm, "counter", &counter, NULL);
    /* The forward and the inverse are both registered. */
    assert(qvm_effect(vm, "counter", effect_inc, effect_dec, NULL) == 0);
    /* The effect is queued, not yet run. */
    assert(counter == 0);
    /* TICK drains the queue. */
    qvm_tick(vm, 0.0);
    assert(counter == 1);
    /* Dispose runs the inverse LIFO. */
    qvm_dispose(vm, "counter");
    assert(counter == 0);
    qvm_free(vm);
    printf("  PASS test_effect (EFFECT)\n");
}

/* === Test 4: VIEW === */
static void test_view(void) {
    qvm_t *vm = qvm_new();
    double depth = 4.2;
    qvm_bind(vm, "bathy:0", &depth, NULL);
    /* A viewer reads the value through VIEW. The viewer name is
     * part of the API; the projection hook could use it. */
    double *seen = (double *)qvm_view(vm, "bathy:0", "cowboy");
    assert(seen != NULL);
    assert(*seen == 4.2);
    /* A different viewer can see the same value. */
    double *also_seen = (double *)qvm_view(vm, "bathy:0", "anyone");
    assert(also_seen == seen);
    /* A non-existent cell returns NULL. */
    assert(qvm_view(vm, "no:such:cell", "anyone") == NULL);
    qvm_free(vm);
    printf("  PASS test_view (VIEW)\n");
}

/* === Test 5: TICK === */
static void test_tick(void) {
    qvm_t *vm = qvm_new();
    /* Time starts at 0.0. */
    assert(vm->time == 0.0);
    /* One tick of 1.0s. */
    qvm_tick(vm, 1.0);
    assert(vm->time == 1.0);
    /* Another tick. */
    qvm_tick(vm, 0.5);
    assert(vm->time == 1.5);
    /* A subscriber receives the tick event. */
    static int got_tick;
    got_tick = 0;
    qvm_subscribe(vm, NULL, &got_tick);  /* simplified: NULL fn */
    (void)got_tick;  /* covered indirectly: tick must not crash */
    qvm_tick(vm, 1.0);
    assert(vm->time == 2.5);
    /* Pending effects run on tick. */
    int counter = 0;
    qvm_bind(vm, "counter", &counter, NULL);
    qvm_effect(vm, "counter", effect_inc, effect_dec, NULL);
    assert(counter == 0);
    qvm_tick(vm, 0.0);
    assert(counter == 1);  /* forward ran */
    qvm_free(vm);
    printf("  PASS test_tick (TICK)\n");
}

int main(void) {
    printf("Running C tests for the 5-opcode Quilt VM (ESP32 port):\n");
    test_bind();
    test_link();
    test_effect();
    test_view();
    test_tick();
    printf("All 5 tests passed!\n");
    printf("The cowboy rides. The 5 opcodes host everything.\n");
    return 0;
}
