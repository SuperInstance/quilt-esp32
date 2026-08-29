/* cells/oil-pressure/twin_config.h -- TWIN-PORT LANE configuration.
 *
 * ==== PLACEHOLDER -- CLEARLY MARKED =======================================
 * This repo has no existing WiFi-credential pattern: every lane so far is
 * radio dark (WiFi.mode(WIFI_OFF), BT stopped -- see the reflex/nmea/
 * eileen lanes). Until a repo-wide secrets pattern lands, this file is
 * the marked placeholder. Defaults ship DARK: the snap transport is the
 * UART ledger line only, which is exactly what the deadband-snap stub
 * needs to stay verifiable without hardware.
 *
 * To light the twin port, override at build time (preferred -- no real
 * credentials in git):
 *   build_flags =
 *       -DTWIN_RADIO_EN=1
 *       -DTWIN_WIFI_SSID='"your-ssid"'
 *       -DTWIN_WIFI_PASS='"your-pass"'
 *       -DTWIN_HTTP_URL='"http://192.168.1.10:8080/quilt"'
 * or edit the defaults below with throwaway values. Never commit real
 * credentials; commit the placeholders.
 * =========================================================================
 */
#ifndef TWIN_CONFIG_H
#define TWIN_CONFIG_H

/* snap.twin_endpoint (from the cell): the logical twin address carried in
 * every post payload. The cell is the source of truth for this string. */
#ifndef TWIN_ENDPOINT_LOGICAL
#define TWIN_ENDPOINT_LOGICAL "twin://game/oil-pressure-port/psi"
#endif

/* HTTP bridge to the game twin (the deadband-snap peer). Empty = no
 * network post, UART ledger only. PLACEHOLDER value. */
#ifndef TWIN_HTTP_URL
#define TWIN_HTTP_URL ""
#endif

/* WiFi credentials for the bridge. PLACEHOLDER values -- see header. */
#ifndef TWIN_WIFI_SSID
#define TWIN_WIFI_SSID ""
#endif
#ifndef TWIN_WIFI_PASS
#define TWIN_WIFI_PASS ""
#endif

/* radio master switch. 0 (default) = radio dark, the repo convention:
 * the stub still logs both books to UART; no radio ever keyed. */
#ifndef TWIN_RADIO_EN
#define TWIN_RADIO_EN 0
#endif

/* join timeout for the one-shot WiFi begin (ms) */
#ifndef TWIN_WIFI_TIMEOUT_MS
#define TWIN_WIFI_TIMEOUT_MS 10000UL
#endif

/* per-post HTTP budget (ms). The transport never blocks the tick loop:
 * snap posts are fire-once, no retry -- redelivery is the ledger's job. */
#ifndef TWIN_POST_TIMEOUT_MS
#define TWIN_POST_TIMEOUT_MS 250
#endif

#endif /* TWIN_CONFIG_H */
