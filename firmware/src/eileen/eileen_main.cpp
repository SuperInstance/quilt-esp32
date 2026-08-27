/* src/eileen/eileen_main.cpp — THE EILEEN on metal, the fourth resolution.
 * Ten named pieces, one joint: the stem's sentences advance the day, the
 * chain reads keel->figurehead in manifest order, the keel blinks the RGB
 * LED at ♩=60 (half-second on, half-second off — the same tempo movement I
 * of the bread score; the same blink first flashed 2026-08-26 13:19).
 * The figurehead closes on the keel: the days grew from the keel.
 * No cloud, no model, no WiFi, radio dark. Receipt at boot.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"

extern "C" {
#include "nmea.h"
#include "eileen_limb.h"
#include "eileen_qm.h"
}

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

static nmea_ctx_t nmea;
static qvm_t *eileen_vm = NULL;
static eileen_cells_t cells;
static unsigned long days = 0;
static unsigned long last_hb = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    WiFi.mode(WIFI_OFF);
    btStop();

    neopixelWrite(RGB_PIN, 0, 0, 0);

    if (eileen_limb_init(&eileen_vm, &cells) != 0) {
        Serial.println("{\"ok\":false,\"error\":\"eileen_limb_init\"}");
        return;
    }
    nmea_reset(&nmea);

    Serial.println("THE EILEEN — a vessel built of days (metal resolution)");
    Serial.printf("receipt: sha256:%s (%d pieces, joint keel->figurehead)\n",
                  EILEEN_QM_SHA256, EILEEN_QM_N_CELLS);
    Serial.println("keel blinks at 60 BPM — feed NMEA sentences to advance the day");
}

void loop() {
    /* the stem: one character at a time, exactly as the water arrives */
    while (Serial.available()) {
        int rc = nmea_feed(&nmea, (char)Serial.read());
        if (rc == 1) {
            /* sentence survived the tally: the day advances */
            char resp[EILEEN_RESP_MAX];
            eileen_day_t day;
            int served = eileen_limb_serve(eileen_vm, &cells, &day, resp);
            if (served == 0) {
                days++;
                Serial.printf("{\"day\":%lu,\"stem\":\"%s\",\"sheerboard\":%d,"
                              "\"figurehead\":%d,\"fired\":%d}\n",
                              days, nmea.out.addr, day.v.sheerboard, day.v.figurehead,
                              day.fired);
            }
        }
    }

    /* the keel: ♩=60 — half-second green, half-second dark */
    int on = eileen_keel_phase(millis());
    neopixelWrite(RGB_PIN, 0, on ? 32 : 0, 0);

    if (millis() - last_hb > 10000) {
        last_hb = millis();
        Serial.printf("[hb] days=%lu millis=%lu keel=%d\n",
                      days, millis(), cells.keel);
    }
}
