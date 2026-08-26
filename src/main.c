/* main.c — Entry point: init WiFi, init ESP-NOW, init VM, register
 * callbacks, run the tick loop.
 *
 * The herd of ESP32s is the runtime. Each board runs this same
 * firmware. Each board binds a small set of cells (bathy:0,
 * tide:current, viewpoint). Each board broadcasts BINDs to its
 * peers so the cell-graph spans the air. Each board ticks every
 * second.
 *
 * The 5 opcodes (BIND, LINK, EFFECT, VIEW, TICK) are the runtime.
 * C is the desert. The ESP32 is the open range.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "quilt_vm.h"
#include "esp_now_mesh.h"

#define QVM_MAIN_TAG "qvm_main"

/* The herd-wide shared key. 16 bytes, all zero = no encryption.
 * Set this to a 16-byte secret in production so only the herd
 * can speak the protocol. */
static const uint8_t HERD_KEY[16] = {
    /* For a real deployment, fill this with a shared secret:
     * 0x01, 0x02, 0x03, ... (16 bytes).
     * The skeleton ships with all zeros (no crypto) so you can
     * flash two boards and have them talk immediately. */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

/* The mesh state. Held at file scope so the FreeRTOS task can
 * reach it. */
static qvm_mesh_t g_mesh;

/* The VM. One per board. Each board's VM is the local slice of
 * the herd's cell-graph. */
static qvm_t *g_vm = NULL;

/* The cell values. Static so their addresses are stable; we
 * hand pointers to the VM, which holds them until dispose. */
static double g_bathy    = 4.2;   /* the water depth at this station */
static char   g_tide[16] = "flooding";  /* current tide state */
static char   g_view[32] = "north"; /* where the lookout is facing */

/* The cell-graph edges. BIND first, then LINK. */
static void qvm_main_register_cells(void) {
    /* Quilt cell: bathy:0 — the cowboy's sounding */
    qvm_bind(g_vm, "bathy:0",        &g_bathy, NULL);
    /* Bay dance: tide:current — the water's state */
    qvm_bind(g_vm, "tide:current",   g_tide,   NULL);
    /* The viewpoint — where the lookout faces */
    qvm_bind(g_vm, "viewpoint",      g_view,   NULL);
}

/* The polyformalism says: bathy:0 depends on tide:current.
 * In cowboy terms: the depth reading wobbles with the tide. */
static void qvm_main_register_links(void) {
    qvm_link(g_vm, "bathy:0", "tide:current", "depends_on");
    qvm_link(g_vm, "viewpoint", "bathy:0",    "observes");
}

/* EFFECT forward/inverse on bathy:0. forward=inc, inverse=dec.
 * Demonstrates the reversible change. The mesh layer broadcasts
 * an EFFECT notification when this runs. */
static void effect_inc(qvm_thing_t *t, void *arg) {
    (void)arg;
    double *v = (double *)qvm_thing_get(t);
    if (v) (*v) += 0.1;
}
static void effect_dec(qvm_thing_t *t, void *arg) {
    (void)arg;
    double *v = (double *)qvm_thing_get(t);
    if (v) (*v) -= 0.1;
}

static void qvm_main_register_effects(void) {
    qvm_effect(g_vm, "bathy:0", effect_inc, effect_dec, NULL);
}

/* Tick subscriber — the bus. Each tick, we log the world. */
static int g_tick_count = 0;
static void tick_subscriber(qvm_event_t *event, void *arg) {
    (void)event; (void)arg;
    g_tick_count++;
    double *bathy = (double *)qvm_view(g_vm, "bathy:0", "anyone");
    ESP_LOGI(QVM_MAIN_TAG, "tick #%d: bathy:0 = %.2f, tide = %s, view = %s",
             g_tick_count,
             bathy ? *bathy : 0.0,
             (char *)qvm_view(g_vm, "tide:current", "anyone"),
             (char *)qvm_view(g_vm, "viewpoint", "anyone"));
}

static void qvm_main_register_subscribers(void) {
    qvm_subscribe(g_vm, tick_subscriber, NULL);
}

/* Broadcast a TICK to peers. We do this every cycle, alongside
 * the local qvm_tick, so the herd stays roughly in sync. */
static void qvm_main_one_cycle(void) {
    /* Drain any inbound ESP-NOW frames (they auto-update the
     * VM inside the recv callback). */
    /* Advance the local VM by 1.0s. */
    qvm_tick(g_vm, 1.0);
    /* Broadcast a tick to the herd. */
    qvm_mesh_send_tick(&g_mesh, 1.0);
}

/* The main task. Lives forever. */
static void qvm_main_task(void *arg) {
    (void)arg;
    ESP_LOGI(QVM_MAIN_TAG, "Quilt VM main task running.");
    while (1) {
        qvm_main_one_cycle();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Periodic BIND broadcast — announce our cells to the herd
 * every 5 seconds. In a real deployment, BINDs would be
 * idempotent; qvm_bind will simply no-op if the cell already
 * exists in the peer's VM. */
static void qvm_main_announce_task(void *arg) {
    (void)arg;
    while (1) {
        qvm_mesh_send_bind(&g_mesh, "bathy:0");
        qvm_mesh_send_bind(&g_mesh, "tide:current");
        qvm_mesh_send_bind(&g_mesh, "viewpoint");
        qvm_mesh_send_link(&g_mesh, "bathy:0",    "tide:current", "depends_on");
        qvm_mesh_send_link(&g_mesh, "viewpoint",  "bathy:0",      "observes");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_LOGI(QVM_MAIN_TAG, "Quilt VM on ESP32. The 5 opcodes. The herd.");

    /* 1. VM */
    g_vm = qvm_new();
    if (!g_vm) {
        ESP_LOGE(QVM_MAIN_TAG, "qvm_new failed");
        return;
    }
    qvm_main_register_cells();
    qvm_main_register_links();
    qvm_main_register_effects();
    qvm_main_register_subscribers();

    /* 2. Mesh (WiFi + ESP-NOW) */
    if (qvm_mesh_init(&g_mesh, g_vm, HERD_KEY) != 0) {
        ESP_LOGE(QVM_MAIN_TAG, "qvm_mesh_init failed");
        return;
    }

    /* 3. Announce our cells to the herd, then start ticking. */
    qvm_mesh_send_bind(&g_mesh, "bathy:0");
    qvm_mesh_send_bind(&g_mesh, "tide:current");
    qvm_mesh_send_bind(&g_mesh, "viewpoint");
    qvm_mesh_send_link(&g_mesh, "bathy:0",    "tide:current", "depends_on");
    qvm_mesh_send_link(&g_mesh, "viewpoint",  "bathy:0",      "observes");

    /* 4. Tasks. */
    xTaskCreate(qvm_main_task,        "qvm_tick",  4096, NULL, 5, NULL);
    xTaskCreate(qvm_main_announce_task,"qvm_anno",  4096, NULL, 4, NULL);

    /* The cowboy rides. */
    ESP_LOGI(QVM_MAIN_TAG, "The cowboy rides the VM.");
    ESP_LOGI(QVM_MAIN_TAG, "The 5 opcodes host everything.");
    ESP_LOGI(QVM_MAIN_TAG, "The composition is the value.");
}
