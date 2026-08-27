/* src/nmea/nmea_main.cpp — the vessel limb on ESP32-S3: NMEA 0183 in on
 * UART0, sheet cells + band rules out, alert LED + pin. No cloud, no
 * model, radio dark.
 *
 *   UART0 (Serial, 115200 8N1) is the sentence source:
 *     — on the desk: the USB-serial adapter, or the replay harness
 *       (cat vessel-stream.txt > /dev/ttyACM0 works; the board's own USB
 *       CDC and UART0 share the port on the S3 DevKitC-1)
 *     — at sea: the instrument feed (GPS/heading/depth/log mux). Most
 *       marine instruments talk 38400 8N1 — swap SERIAL_BAUD below, or
 *       move to a second UART: Serial2.begin(38400, SERIAL_8N1, 16, 17)
 *       and feed Serial2.read() bytes into the same nmea_feed loop.
 *
 * Every byte goes through nmea_feed() (byte-at-a-time, no second pass);
 * complete core sentences absorb into the sheet, the rule table judges,
 * the canon EFFECT+TICK+VIEW path lands the alert, and the LED speaks:
 *     green steady = clear · amber steady = watch · red flash = alert
 * ALERT_PIN (GPIO4, documented) goes HIGH while alert==2 — buzzer/relay
 * for the engine room. One JSON line per sentence over the same port.
 * Banner + [hb] heartbeat every 10 s (no interactive commands — the byte
 * path stays pure parser, nothing else to misfire on a noisy feed).
 */
#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"

extern "C" {
#include "nmea.h"
#include "nmea_limb.h"
}

#define NMEA_LIMB_VERSION "nmea-limb v1"
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200   /* desk: USB CDC. sea: 38400 (see header) */
#endif
#ifndef ALERT_PIN
#define ALERT_PIN 4          /* HIGH while alert==2 — buzzer/relay line */
#endif

static nmea_ctx_t nmea;
static qvm_t *vm = NULL;
static vessel_cells_t cells = {0};
static unsigned long serves = 0;

static void limb_led(int alert, int flash_phase)
{
#ifdef RGB_PIN
    /* S3 DevKitC-1 WS2812: green steady / amber steady / red flash 2 Hz */
    if (alert >= 2)
        neopixelWrite(RGB_PIN, flash_phase ? 60 : 0, 0, 0);
    else if (alert == 1)
        neopixelWrite(RGB_PIN, 48, 24, 0);
    else
        neopixelWrite(RGB_PIN, 0, 48, 0);
#else
    /* classic ESP32 GPIO2: on = alert (flash), dim-flicker = watch */
    (void)flash_phase;
    digitalWrite(LED_BUILTIN, alert >= 2 ? (flash_phase ? HIGH : LOW)
                                          : (alert == 1 ? HIGH : LOW));
#endif
    digitalWrite(ALERT_PIN, alert >= 2 ? HIGH : LOW);
}

static void print_banner()
{
    Serial.println(NMEA_LIMB_VERSION " — the boat's instruments feeding the quilt — no cloud, no model, no radio");
    Serial.printf("rules: vessel-demo.qm v%d mint-receipt sha256: %s\n",
                  VESSEL_QM_VERSION, VESSEL_QM_SHA256);
    Serial.println("fixed-point: micro-units (1 unit = 1e-6), integer-only — no floats on target; ddmm.mmmm → µdeg (int64 intermediate)");
    Serial.printf("geofence lat[%ld,%ld] lon[%ld,%ld]+%ldµdeg · depth alert<%ldµm warn<%ldµm · drift warn>%ldµkn alert>%ldµkn · nofix sev %d\n",
                  (long)VESSEL_QM_GEOFENCE.lat_lo, (long)VESSEL_QM_GEOFENCE.lat_hi,
                  (long)VESSEL_QM_GEOFENCE.lon_lo, (long)VESSEL_QM_GEOFENCE.lon_hi,
                  (long)VESSEL_QM_GEOFENCE.margin,
                  (long)VESSEL_QM_DEPTH_ALERT_BELOW, (long)VESSEL_QM_DEPTH_WARN_BELOW,
                  (long)VESSEL_QM_DRIFT_WARN_ABOVE, (long)VESSEL_QM_DRIFT_ALERT_ABOVE,
                  VESSEL_QM_NOFIX_SEV);
    Serial.println("cells: ais.position.lat/lon own.fix own.heading own.depth own.sog own.wspeed — BIND/LINK/EFFECT/VIEW/TICK (Paper 211)");
    Serial.println("feed: NMEA 0183 on UART0 (115200 desk / 38400 sea) — GGA RMC HDT DBT VHW; checksum required");
    Serial.println("led: green=clear amber=watch red-flash=alert · pin GPIO4 HIGH on alert");
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    pinMode(ALERT_PIN, OUTPUT);
    digitalWrite(ALERT_PIN, LOW);
#ifdef RGB_PIN
    neopixelWrite(RGB_PIN, 0, 0, 0);
#else
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    /* radio dark — the limb judges alone; nothing leaves the board */
    WiFi.mode(WIFI_OFF);
    btStop();
    delay(50);

    nmea_reset(&nmea);
    if (vessel_limb_init(&vm, &cells) != 0) {
        Serial.println("{\"ok\":false,\"error\":\"vessel_limb_init\"}");
        return;
    }
    limb_led(0, 0);
    print_banner();
    Serial.println("radio: WiFi off, BT stopped — dark");
    Serial.println("ready — sentences in, JSON lines out");
}

void loop()
{
    static unsigned long last_flash = 0, last_hb = 0;
    static int flash_phase = 0;
    static vessel_judgment_t last_j = {0, 0, 0, 0, 0};

    while (Serial.available()) {
        char c = (char)Serial.read();
        int rc = nmea_feed(&nmea, c);
        if (rc == NMEA_DONE && nmea.out.known) {
            vessel_limb_absorb(&cells, &nmea.out);
            char resp[VESSEL_RESP_MAX];
            if (vessel_limb_serve(vm, &cells, &last_j, resp) == 0) {
                const char *led = last_j.alert == 2 ? "red-flash"
                                : last_j.alert == 1 ? "amber" : "green";
                Serial.printf("{\"i\":%lu,\"addr\":\"%s\",\"mode\":\"served\","
                              "\"response\":%s,\"led\":\"%s\"}\n",
                              serves, nmea.out.addr, resp, led);
                limb_led(last_j.alert, flash_phase);
            } else {
                Serial.printf("{\"i\":%lu,\"addr\":\"%s\",\"mode\":\"serve-error\"}\n",
                              serves, nmea.out.addr);
            }
            serves++;
        } else if (rc == NMEA_DONE) {
            Serial.printf("{\"i\":%lu,\"addr\":\"%s\",\"mode\":\"unknown\"}\n",
                          serves, nmea.out.addr);
            serves++;
        } else if (rc == NMEA_BAD_CK) {
            Serial.printf("{\"i\":%lu,\"mode\":\"bad-ck\"}\n", serves++);
        } else if (rc == NMEA_BAD_FMT) {
            Serial.printf("{\"i\":%lu,\"mode\":\"bad-fmt\"}\n", serves++);
        } else if (rc == NMEA_OVERFLOW) {
            Serial.printf("{\"i\":%lu,\"mode\":\"overflow\"}\n", serves++);
        }
    }

    /* red flash cadence 2 Hz; amber/green steady */
    unsigned long now = millis();
    if (now - last_flash >= 250) {
        last_flash = now;
        flash_phase = !flash_phase;
        limb_led(last_j.alert, flash_phase);
    }
    if (now - last_hb >= 10000) {
        last_hb = now;
        Serial.printf("[hb] serves=%lu ok=%lu unk=%lu badck=%lu badfmt=%lu "
                      "ovf=%lu alert=%d millis=%lu\n",
                      serves, (unsigned long)nmea.n_ok,
                      (unsigned long)nmea.n_unknown,
                      (unsigned long)nmea.n_bad_ck,
                      (unsigned long)nmea.n_bad_fmt,
                      (unsigned long)nmea.n_overflow,
                      last_j.alert, now);
    }
    delay(1);   /* gentle poll — parser + judge are O(line) integer math */
}
