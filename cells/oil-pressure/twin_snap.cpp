/* cells/oil-pressure/twin_snap.cpp -- the deadband-snap client stub.
 *
 * Strong override of the generated cell's weak transport seam
 * (tower_twin_post). SEMANTIC-TOWER 5.4: the post IS the ledger entry --
 * one QUF state line, both books -- so the stub's one obligation is to
 * write the local book (UART, always, dark or not) and, when the twin
 * endpoint is configured and the radio is lit, hand the same line to the
 * game twin over HTTP. Fire-once, no retry, no blocking: redelivery
 * safety is the ledger's job (FOUNDATION D3), not the cell's.
 *
 * Radio stays DARK by default (twin_config.h, the repo convention). The
 * stub is the seam where a real transport (MQTT, quilt-mesh flit,
 * fabric) plugs in later -- same one function.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "twin_config.h"
#include "twin_snap.h"

extern "C" {
#include "tower_port.h"
}

#if TWIN_RADIO_EN
#include <HTTPClient.h>
#endif

static int  g_radio_up = 0;
static long g_http_posts = 0;
static long g_http_drops = 0;

void twin_snap_client_begin(void)
{
#if TWIN_RADIO_EN
    if (TWIN_WIFI_SSID[0] == '\0') {
        Serial.println("twin: TWIN_RADIO_EN=1 but TWIN_WIFI_SSID empty -- staying dark");
        WiFi.mode(WIFI_OFF);
        return;
    }
    Serial.printf("twin: joining '%s' ...\n", TWIN_WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(TWIN_WIFI_SSID, TWIN_WIFI_PASS);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - t0 < TWIN_WIFI_TIMEOUT_MS) {
        delay(50);
    }
    g_radio_up = (WiFi.status() == WL_CONNECTED);
    if (g_radio_up) {
        Serial.print("twin: up, bridge ");
        Serial.println(TWIN_HTTP_URL);
    } else {
        Serial.println("twin: join failed -- UART ledger only");
    }
#else
    WiFi.mode(WIFI_OFF); /* radio dark, the repo default */
    Serial.println("twin: radio dark (TWIN_RADIO_EN=0) -- UART ledger only");
#endif
}

int  twin_snap_client_up(void)   { return g_radio_up; }
void twin_snap_client_tick(void) { /* stub: no reconnect, by design */ }

/* the strong transport: called by tower_tick() only on SNAP */
extern "C" void tower_twin_post(const char *endpoint, const char *state_line)
{
    /* local book: always written, dark or not */
    Serial.print("[snap] ");
    Serial.print(endpoint);
    Serial.print(' ');
    Serial.println(state_line);

#if TWIN_RADIO_EN
    if (g_radio_up && TWIN_HTTP_URL[0] != '\0') {
        HTTPClient http;
        http.setConnectTimeout((int)TWIN_POST_TIMEOUT_MS);
        http.setTimeout((int)TWIN_POST_TIMEOUT_MS);
        if (http.begin(TWIN_HTTP_URL)) {
            int code = http.POST(state_line); /* raw QUF line, text/plain */
            http.end();
            if (code > 0) {
                g_http_posts++;
            } else {
                g_http_drops++; /* fire-once: the drop is visible, not retried */
            }
        } else {
            g_http_drops++;
        }
    }
#else
    (void)g_http_posts;
    (void)g_http_drops;
#endif
}
