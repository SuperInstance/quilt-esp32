/* esp_now_mesh.c — ESP-NOW peer-to-peer mesh for the Quilt VM.
 *
 * The herd of ESP32s is the runtime. Each board holds some
 * cells locally. ESP-NOW propagates BINDs, LINKs, EFFECTs,
 * and TICKs to neighbors so the cell-graph spans the air.
 *
 * Style note: we wrap every ESP-IDF call in a thin local
 * function so the rest of the file does not have to know
 * about the headers. quilt_vm.c has no ESP-specific types;
 * neither does this file's outer surface.
 */

#include "esp_now_mesh.h"

#include <stdio.h>
#include <string.h>

/* ESP-IDF headers — only included here, not in quilt_vm.c. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define QVM_MESH_TAG "qvm_mesh"
#define QVM_MESH_MAX_PEERS 16
#define QVM_MESH_BCAST {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/* The single mesh instance held by the project. We keep this
 * module-level because ESP-NOW callbacks are C function
 * pointers and cannot carry user data. */
static qvm_mesh_t *g_mesh = NULL;

/* --- Forward declarations for internal helpers --- */
static void qvm_mesh_on_send(const uint8_t *mac_addr, esp_now_send_status_t status);
static void qvm_mesh_on_recv(const uint8_t *mac_addr, const uint8_t *data, int len);
static int  qvm_mesh_remember_peer(const uint8_t mac[6]);
static int  qvm_mesh_add_esp_now_peer(const uint8_t mac[6]);

/* === Wire helpers === */

int qvm_mesh_pack_frame(uint8_t opcode,
                        const char *name, const char *b, const char *type,
                        uint8_t *outbuf, size_t outbuf_size) {
    if (!outbuf) return -1;
    size_t name_l = name ? strlen(name) + 1 : 0;
    size_t b_l    = b    ? strlen(b)    + 1 : 0;
    size_t type_l = type ? strlen(type) + 1 : 0;

    if (name_l > 0xFFFF || b_l > 0xFFFF || type_l > 0xFFFF) return -1;

    size_t header = sizeof(qvm_mesh_frame_t);
    if (header + name_l + b_l + type_l > outbuf_size) return -1;

    qvm_mesh_frame_t *f = (qvm_mesh_frame_t *)outbuf;
    f->opcode   = opcode;
    f->reserved = 0;
    f->name_len = (uint16_t)name_l;
    f->b_len    = (uint16_t)b_l;
    f->type_len = (uint16_t)type_l;

    uint8_t *p = outbuf + header;
    if (name_l) { memcpy(p, name, name_l); p += name_l; }
    if (b_l)    { memcpy(p, b,    b_l);    p += b_l;    }
    if (type_l) { memcpy(p, type, type_l); p += type_l; }

    return (int)(header + name_l + b_l + type_l);
}

int qvm_mesh_unpack_frame(const uint8_t *buf, size_t len,
                          uint8_t *opcode,
                          char *name, size_t name_size,
                          char *b,    size_t b_size,
                          char *type, size_t type_size) {
    if (!buf || len < sizeof(qvm_mesh_frame_t)) return -1;
    const qvm_mesh_frame_t *f = (const qvm_mesh_frame_t *)buf;
    if (f->name_len + f->b_len + f->type_len + sizeof(qvm_mesh_frame_t) > len) {
        return -1;
    }
    if (opcode) *opcode = f->opcode;
    const uint8_t *p = buf + sizeof(qvm_mesh_frame_t);
    if (f->name_len) {
        if (name_size < f->name_len) return -1;
        memcpy(name, p, f->name_len - 1);
        name[f->name_len - 1] = '\0';
        p += f->name_len;
    } else if (name && name_size) { name[0] = '\0'; }
    if (f->b_len) {
        if (b_size < f->b_len) return -1;
        memcpy(b, p, f->b_len - 1);
        b[f->b_len - 1] = '\0';
        p += f->b_len;
    } else if (b && b_size) { b[0] = '\0'; }
    if (f->type_len) {
        if (type_size < f->type_len) return -1;
        memcpy(type, p, f->type_len - 1);
        type[f->type_len - 1] = '\0';
    } else if (type && type_size) { type[0] = '\0'; }
    return 0;
}

void qvm_mesh_mac_str(const uint8_t mac[6], char *out, size_t out_size) {
    if (!out || out_size < 18) return;
    snprintf(out, out_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* === Peer table === */

static int qvm_mesh_remember_peer(const uint8_t mac[6]) {
    if (!g_mesh) return -1;
    for (size_t i = 0; i < g_mesh->n_peers; i++) {
        if (memcmp(g_mesh->peers[i].mac, mac, 6) == 0) {
            g_mesh->peers[i].seen = 1;
            return 0;
        }
    }
    if (g_mesh->n_peers >= QVM_MESH_MAX_PEERS) return -1;
    memcpy(g_mesh->peers[g_mesh->n_peers].mac, mac, 6);
    g_mesh->peers[g_mesh->n_peers].seen = 1;
    g_mesh->n_peers++;
    /* Also register the peer with ESP-NOW so we can unicast to it. */
    return qvm_mesh_add_esp_now_peer(mac);
}

static int qvm_mesh_add_esp_now_peer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) return 0;
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;            /* use the channel we are on */
    peer.encrypt = memcmp(g_mesh->herd_key, (uint8_t[16]){0}, 16) != 0;
    if (peer.encrypt) {
        memcpy(peer.lmk, g_mesh->herd_key, 16);
    }
    return esp_now_add_peer(&peer);
}

static const uint8_t *qvm_mesh_find_peer_for_name(const char *name) {
    /* The mesh does not yet know which peer owns which cell.
     * The 1.0 skeleton uses broadcast for everything, so this
     * just returns the broadcast address. A real deployment
     * would maintain a name->MAC map (e.g. by including the
     * owner's MAC in every BIND broadcast it forwards). */
    (void)name;
    static const uint8_t bcast[6] = QVM_MESH_BCAST;
    return bcast;
}

/* === ESP-NOW send / recv callbacks === */

static void qvm_mesh_on_send(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (!g_mesh) return;
    char macstr[18];
    qvm_mesh_mac_str(mac_addr, macstr);
    ESP_LOGI(QVM_MESH_TAG, "send to %s -> %s",
             macstr,
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

static void qvm_mesh_on_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (!g_mesh || !data || len <= 0) return;
    char macstr[18];
    qvm_mesh_mac_str(mac_addr, macstr);
    ESP_LOGI(QVM_MESH_TAG, "recv %d bytes from %s", len, macstr);

    /* Record the sender. */
    qvm_mesh_remember_peer(mac_addr);

    uint8_t opcode;
    char name[64], b[64], type[64];
    if (qvm_mesh_unpack_frame(data, (size_t)len, &opcode,
                              name, sizeof(name),
                              b,    sizeof(b),
                              type, sizeof(type)) != 0) {
        ESP_LOGW(QVM_MESH_TAG, "malformed frame from %s", macstr);
        return;
    }

    switch (opcode) {
        case QVM_MSG_BIND:
            /* A peer has registered a cell. Store it locally
             * so the cell-graph spans the air. */
            if (qvm_bind(g_mesh->vm, name, NULL, NULL) == 0) {
                ESP_LOGI(QVM_MESH_TAG, "  -> BIND %s", name);
            }
            break;
        case QVM_MSG_LINK: {
            /* A peer drew an arrow. Recreate it locally. The
             * reverse link is auto-registered by qvm_link. */
            if (qvm_link(g_mesh->vm, name, b, type) == 0) {
                ESP_LOGI(QVM_MESH_TAG, "  -> LINK %s --%s--> %s",
                         name, type, b);
            }
            break;
        }
        case QVM_MSG_EFFECT:
            /* A peer ran an effect. We could mirror it; the
             * skeleton just logs. */
            ESP_LOGI(QVM_MESH_TAG, "  -> EFFECT %s", name);
            break;
        case QVM_MSG_VIEW:
            /* A peer asked for a view. The skeleton ignores. */
            ESP_LOGI(QVM_MESH_TAG, "  -> VIEW %s by %s", name, b);
            break;
        case QVM_MSG_TICK:
            /* A peer ticked. Bump our local time so we stay
             * roughly in sync. */
            g_mesh->vm->time += 0.001;  /* small nudge */
            break;
        default:
            ESP_LOGW(QVM_MESH_TAG, "  -> unknown opcode %u", opcode);
    }
}

/* === Init === */

int qvm_mesh_init(qvm_mesh_t *m, qvm_t *vm, const uint8_t herd_key[16]) {
    if (!m || !vm) return -1;
    memset(m, 0, sizeof(*m));
    m->vm = vm;
    if (herd_key) memcpy(m->herd_key, herd_key, 16);

    /* NVS is required for WiFi. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Capture our own MAC for logging. */
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, m->my_mac));
    char mystr[18];
    qvm_mesh_mac_str(m->my_mac, mystr);
    ESP_LOGI(QVM_MESH_TAG, "this board's MAC: %s", mystr);

    /* Bring up ESP-NOW. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(qvm_mesh_on_send));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(qvm_mesh_on_recv));

    /* Register the broadcast peer (used for BIND, EFFECT, TICK). */
    const uint8_t bcast[6] = QVM_MESH_BCAST;
    int use_crypto = memcmp(m->herd_key, (uint8_t[16]){0}, 16) != 0;
    {
        esp_now_peer_info_t peer;
        memset(&peer, 0, sizeof(peer));
        memcpy(peer.peer_addr, bcast, 6);
        peer.channel = 0;
        peer.encrypt = use_crypto;
        if (peer.encrypt) {
            memcpy(peer.lmk, m->herd_key, 16);
        }
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    }

    g_mesh = m;
    m->initialised = 1;
    ESP_LOGI(QVM_MESH_TAG, "ESP-NOW mesh up. Herd key: %s",
             use_crypto ? "ENABLED" : "DISABLED");
    return 0;
}

/* === Senders === */

int qvm_mesh_send_bind(qvm_mesh_t *m, const char *name) {
    if (!m || !m->initialised || !name) return -1;
    uint8_t buf[250];
    int n = qvm_mesh_pack_frame(QVM_MSG_BIND, name, NULL, NULL, buf, sizeof(buf));
    if (n < 0) return -1;
    const uint8_t bcast[6] = QVM_MESH_BCAST;
    return esp_now_send(bcast, buf, (size_t)n);
}

int qvm_mesh_send_link(qvm_mesh_t *m, const char *a, const char *b,
                       const char *type) {
    if (!m || !m->initialised || !a || !b || !type) return -1;
    uint8_t buf[250];
    int n = qvm_mesh_pack_frame(QVM_MSG_LINK, a, b, type, buf, sizeof(buf));
    if (n < 0) return -1;
    const uint8_t *target = qvm_mesh_find_peer_for_name(b);
    return esp_now_send(target, buf, (size_t)n);
}

int qvm_mesh_send_effect(qvm_mesh_t *m, const char *target) {
    if (!m || !m->initialised || !target) return -1;
    uint8_t buf[250];
    int n = qvm_mesh_pack_frame(QVM_MSG_EFFECT, target, NULL, NULL, buf, sizeof(buf));
    if (n < 0) return -1;
    const uint8_t bcast[6] = QVM_MESH_BCAST;
    return esp_now_send(bcast, buf, (size_t)n);
}

int qvm_mesh_send_tick(qvm_mesh_t *m, double dt) {
    if (!m || !m->initialised) return -1;
    uint8_t buf[32];
    char dtbuf[16];
    snprintf(dtbuf, sizeof(dtbuf), "%.4f", dt);
    int n = qvm_mesh_pack_frame(QVM_MSG_TICK, dtbuf, NULL, NULL, buf, sizeof(buf));
    if (n < 0) return -1;
    const uint8_t bcast[6] = QVM_MESH_BCAST;
    return esp_now_send(bcast, buf, (size_t)n);
}
