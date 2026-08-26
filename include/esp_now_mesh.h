/* esp_now_mesh.h — ESP-NOW peer-to-peer mesh for the Quilt VM.
 *
 * The herd of ESP32s is the runtime. Each board holds some
 * cells locally. ESP-NOW propagates BINDs, LINKs, EFFECTs,
 * and TICKs to neighbors so the cell-graph spans the air.
 *
 * Wire protocol (single struct, packed, fits one ESP-NOW
 * frame, max 250 bytes):
 *
 *   uint8_t  opcode;   // 0=BIND, 1=LINK, 2=EFFECT, 3=VIEW, 4=TICK
 *   uint8_t  reserved;
 *   uint16_t name_len;
 *   uint16_t b_len;
 *   uint16_t type_len;
 *   char     names[];  // name\0 b\0 type\0 (variable, packed)
 *
 *   payload_len = name_len + b_len + type_len
 *
 * BIND carries (name) — broadcast.
 * LINK carries (a, b, type) — unicast to the registered peer
 *                            that owns `b`, fallback broadcast.
 * EFFECT carries (target) only — broadcast (the forward/inverse
 *                              are registered locally; the mesh
 *                              just notifies that a change ran).
 * TICK carries no payload — broadcast every N ms.
 *
 * The herd-wide shared key is set once at init. ESP-NOW uses
 * it to encrypt every frame at the radio layer.
 */

#ifndef QUILT_ESP_NOW_MESH_H
#define QUILT_ESP_NOW_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "quilt_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wire opcodes — match the Quilt VM opcodes by number */
typedef enum {
    QVM_MSG_BIND   = 0,
    QVM_MSG_LINK   = 1,
    QVM_MSG_EFFECT = 2,
    QVM_MSG_VIEW   = 3,
    QVM_MSG_TICK   = 4
} qvm_msg_opcode_t;

/* A single mesh frame. The names region is variable-length
 * and lives at the end of the struct. Use mesh_pack_frame()
 * to build one. */
typedef struct {
    uint8_t  opcode;
    uint8_t  reserved;
    uint16_t name_len;
    uint16_t b_len;
    uint16_t type_len;
    /* followed by: name\0 b\0 type\0 */
} qvm_mesh_frame_t;

/* Peer table entry. We track every board we have ever heard
 * from so we can do unicast LINKs back to them. */
typedef struct {
    uint8_t  mac[6];
    int      seen;   /* 1 if we've ever received a frame from this peer */
} qvm_mesh_peer_t;

/* The mesh state. One per device. Held by main.c. */
typedef struct {
    qvm_t *vm;                    /* The local VM */
    qvm_mesh_peer_t peers[16];   /* Tiny peer table; bump if your herd is larger */
    size_t n_peers;
    uint8_t herd_key[16];         /* ESP-NOW shared key (16 bytes, all-zero = no crypto) */
    uint8_t my_mac[6];
    int initialised;
} qvm_mesh_t;

/* Initialise the mesh: WiFi station mode, ESP-NOW, register
 * the send/recv callbacks. Returns 0 on success. */
int qvm_mesh_init(qvm_mesh_t *m, qvm_t *vm, const uint8_t herd_key[16]);

/* Send a BIND over ESP-NOW (broadcast). */
int qvm_mesh_send_bind(qvm_mesh_t *m, const char *name);

/* Send a LINK to the peer that owns `b`. If the target MAC
 * is not known, the frame is broadcast. */
int qvm_mesh_send_link(qvm_mesh_t *m, const char *a, const char *b,
                       const char *type);

/* Send an EFFECT notification (broadcast). */
int qvm_mesh_send_effect(qvm_mesh_t *m, const char *target);

/* Broadcast a TICK to all peers. dt is the time delta in
 * seconds. */
int qvm_mesh_send_tick(qvm_mesh_t *m, double dt);

/* Pack (name, b, type) into outbuf. Returns the frame size,
 * or -1 if the payload would not fit. */
int qvm_mesh_pack_frame(uint8_t opcode,
                        const char *name, const char *b, const char *type,
                        uint8_t *outbuf, size_t outbuf_size);

/* Unpack a frame received from ESP-NOW. Validates lengths.
 * Returns 0 on success; name/b/type are written into
 * caller-provided buffers (caller must size them to fit
 * the worst case). */
int qvm_mesh_unpack_frame(const uint8_t *buf, size_t len,
                          uint8_t *opcode,
                          char *name, size_t name_size,
                          char *b,    size_t b_size,
                          char *type, size_t type_size);

/* Print a peer's MAC in the standard aa:bb:cc:dd:ee:ff form. */
void qvm_mesh_mac_str(const uint8_t mac[6], char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* QUILT_ESP_NOW_MESH_H */
