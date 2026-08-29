/* cells/oil-pressure/oil_pressure_main.cpp -- the TWIN-PORT LANE on metal:
 * the tower-pilot oil-pressure cell (quilt-verilog tools/tower) driving a
 * real ADC pin at the cell's fixed tick. No cloud, no model; radio dark
 * unless twin_config.h lights the twin bridge.
 *
 *   cell      : oil-pressure-port (L0: oil-pressure-port.cell.yaml)
 *   chain     : ADC mV -> moving_median(5) -> psi=(mV-500)*3/80 on the
 *               1/80-psi basis -> whole psi -> deadband judge (D=1 psi,
 *               squared form) -> snap post (QUF state line) on SNAP only
 *   tick      : 100 ms fixed timestep (tick.period_ms), millis()-driven
 *   transport : twin_snap.cpp stub -- UART ledger always, HTTP bridge
 *               only when configured (twin_config.h)
 *
 * Board note (below the cell's horizon): a 0.5-4.5 V transducer exceeds
 * the ESP32-S3 ADC input range -- front it with a divider in board glue
 * and fold the ratio into the mV adapter (or use a 3.3 V-span sensor).
 * analogReadMilliVolts returns calibrated pin mV; the divider constant
 * belongs here, never in the generated cell.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"

extern "C" {
#include "tower_port.h"
#include "twin_snap.h"
}
#include "twin_config.h"

#define OIL_LANE_VERSION "oil-pressure-port v1 (tower twin-port lane)"

#ifndef OIL_ADC_PIN
#define OIL_ADC_PIN 4          /* ADC1_CH3 on the S3 DevKitC-1 (override: -DOIL_ADC_PIN=n) */
#endif
#ifndef OIL_TICK_MS
#define OIL_TICK_MS 100        /* tick.period_ms from the cell -- keep in lockstep */
#endif
#ifndef OIL_HEARTBEAT_TICKS
#define OIL_HEARTBEAT_TICKS 100 /* one QUF heartbeat line per 100 ticks (10 s) */
#endif

/* io[0] glue: raw transducer millivolts. The generated cell's weak
 * default returns 0; this strong override is the only ADC touchpoint. */
extern "C" int32_t tower_adc_read_mV(void)
{
    return (int32_t)analogReadMilliVolts(OIL_ADC_PIN);
}

static void print_banner(void)
{
    Serial.println(OIL_LANE_VERSION " -- tower cell on metal, deadband-snap twin port");
    Serial.println("render: psi = (mV - 500) * 3 / 80, 1/80-psi basis exact; median window 5; range [0,150]; deadband 1 whole psi");
    Serial.printf("tick: %d ms fixed on GPIO%d (ADC) -- endpoint %s\n",
                  OIL_TICK_MS, OIL_ADC_PIN, TWIN_ENDPOINT_LOGICAL);
    Serial.println("contract: agree-to-within-D, snap-on-exceed, reality-wins, log-both-books, all-integer");
}

static unsigned long next_tick_ms = 0;

void setup(void)
{
    Serial.begin(115200);
    delay(200);
    print_banner();

    WiFi.mode(WIFI_OFF);       /* repo convention: dark until the twin */
    btStop();                  /* client explicitly lights the radio   */
    twin_snap_client_begin();

    tower_reset();
    next_tick_ms = millis();
    Serial.println("lane: ticking");
}

void loop(void)
{
    unsigned long now = millis();
    if ((long)(now - next_tick_ms) >= 0) {
        tower_tick(); /* one fixed timestep of the whole chain */
        next_tick_ms += OIL_TICK_MS;
        if ((tower_ticks() % OIL_HEARTBEAT_TICKS) == 0) {
            char line[192]; /* TOWER_QUF_LINE_MAX in the generated cell */
            (void)tower_quf_line(line, sizeof line);
            Serial.print("[quf] ");
            Serial.println(line);
        }
    }
    twin_snap_client_tick();
}
