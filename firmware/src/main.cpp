/* src/main.cpp — limb-blink firmware: ESP32 DevKit V1 blinks GPIO2 driven
 * by the compiled .qm rule table through the real quilt-vm-c. No cloud, no
 * model, no WiFi. pio compiles the serve path as C, so qm_serve.h is
 * wrapped in extern "C" here (headers stay byte-identical to the host
 * build). */
#include <Arduino.h>

extern "C" {
#include "qm_serve.h"
}

/* esp32dev variant (arduino-esp32 2.x) does not define LED_BUILTIN */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 /* GPIO2 on DevKit V1 */
#endif

#ifdef TARGET_ESP32S3
/* S3 DevKitC-1: addressable WS2812 RGB LED, low brightness */
static inline void limb_led_write(int on)
{
    neopixelWrite(RGB_PIN, on ? 0 : 0, on ? 48 : 0, on ? 0 : 0);
}
#define limb_write(led) limb_led_write((led) > 0)
#else
#define limb_write(led) digitalWrite(LED_BUILTIN, (led) > 0 ? HIGH : LOW)
#endif

static qvm_t *vm = NULL;
static unsigned long serves = 0;

/* fixture phases; canon values are canonical JSON of the scalar */
static const char *phase_canon[2] = { "\"on\"", "\"off\"" };

void setup() {
    Serial.begin(115200);
#ifdef TARGET_ESP32S3
    neopixelWrite(RGB_PIN, 0, 0, 0); /* RGB off at boot */
#else
    pinMode(LED_BUILTIN, OUTPUT); /* GPIO2 on DevKit V1 */
#endif
    if (qm_serve_init(&vm) != 0) {
        Serial.println("{\"ok\":false,\"error\":\"qm_serve_init\"}");
        return;
    }
    Serial.println("limb-blink v0.2 — quilt signal cell on ESP32 — no cloud, no model");
    Serial.println("canon: qm_bind/qm_link/qm_effect/qm_view/qm_tick (Paper 211)");
    Serial.printf("facts: rules=%d binds=%d\n", QM_N_RULES, QM_N_BINDS);
}

void loop() {
    /* delay(500) chosen over millis() scheduling: simpler, and this spike
     * has nothing else to do between serves */
    const char *canon = phase_canon[serves % 2];

    QmKv payload = { "phase", canon };
    QmSignal s = { "led-limb", "tick", 1, &payload };
    char mode[16];
    const char *resp = NULL;
    int rc = qm_serve(vm, &s, mode, &resp);

    if (rc == 0) {
        int led = qm_led_from_response(resp);
        if (led >= 0)
            limb_write(led);
        /* miss (led == -1): leave LED unchanged, mode says table-miss */
        Serial.printf("{\"i\":%lu,\"mode\":\"%s\",\"led\":%d}\n",
                      serves, mode, led);
    } else {
        Serial.printf("{\"i\":%lu,\"mode\":\"serve-error\",\"rc\":%d}\n",
                      serves, rc);
    }

    serves++;
    if (serves % 40 == 0) /* every 40 serves = 20 s */
        Serial.printf("[hb] serves=%lu millis=%lu\n", serves, millis());

    delay(500);
}
