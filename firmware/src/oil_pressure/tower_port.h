/* cells/oil-pressure/tower_port.h -- public API of the tower-generated
 * oil-pressure cell (tower_oil_pressure_port.c, the L2 middle layer).
 *
 * The generated TU re-declares these itself (it is self-contained); this
 * header exists so the board glue (oil_pressure_main.cpp), the deadband-
 * snap twin client (twin_snap.cpp), and the host golden-vector test
 * (firmware/host_oil/main.c) can include one stable surface instead of
 * quoting prototypes. If tower-sync.sh regenerates the cell with a new
 * compiler shape, this header is the one hand-written file to re-check.
 *
 * Refresh path (generated-C-as-middle-layer): edit the L0 cell in
 * quilt-verilog/tools/tower/, run tools/tower-sync.sh here, never edit
 * tower_oil_pressure_port.c by hand.
 */
#ifndef TOWER_PORT_H
#define TOWER_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* glue seams (weak in the generated TU -- board files override) */
int32_t  tower_adc_read_mV(void);                    /* io[0]: raw mV    */
void     tower_twin_post(const char *endpoint,       /* snap transport   */
                         const char *state_line);

/* fixed-timestep chain: prefilter -> render -> quantize -> judge -> snap */
void     tower_tick(void);
void     tower_reset(void);

/* QUF state line: the whole cell image as integer key=value pairs */
size_t   tower_quf_line(char *buf, size_t n);

/* qm_view accessors over the flat static image */
int32_t  tower_raw_mV(void);
int32_t  tower_med_mV(void);
int32_t  tower_psi80(void);
int32_t  tower_psi_whole(void);
int32_t  tower_twin_psi(void);
uint32_t tower_posts(void);
int32_t  tower_snap_debt(void);
uint32_t tower_ticks(void);

/* host/game-side seam: stage the twin's belief (verification only) */
void     tower_twin_set_belief(int32_t v);

#ifdef __cplusplus
}
#endif

#endif /* TOWER_PORT_H */
