/* cells/oil-pressure/twin_snap.h -- deadband-snap client stub surface.
 *
 * The generated cell calls tower_twin_post(endpoint, state_line) ONLY on
 * a SNAP verdict; twin_snap.cpp provides the strong definition (a stub
 * transport): the UART ledger line always, an HTTP post to the
 * configurable twin endpoint only when twin_config.h lights the radio.
 * The stub never blocks, never retries -- the tick loop is sacred.
 */
#ifndef TWIN_SNAP_H
#define TWIN_SNAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* one-shot bring-up at boot: join WiFi iff TWIN_RADIO_EN and creds are
 * set; otherwise hold the repo's radio-dark convention. */
void twin_snap_client_begin(void);

/* 1 iff the network transport is ready (always 0 when dark) */
int  twin_snap_client_up(void);

/* per-loop housekeeping hook (stub: no reconnect logic by design) */
void twin_snap_client_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* TWIN_SNAP_H */
