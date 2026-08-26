/* src/reflex/reflex_main.cpp — reflex-arc firmware: the critic's frozen
 * gate on ESP32-S3, fed real critique vectors over UART.
 *
 *   radio dark (WiFi off, BT stopped) · integer-only judge (critic_gate.c)
 *   · mint-receipt sha256 of gate-bands.json printed at boot · per-vector
 *   verdict + latency in µs · dissent lines for near-edge readings.
 *
 * UART protocol, 115200 8N1, one line each:
 *   host → board:  V <id> <f1..f6>        six channel readings in µ
 *                   P <id> <chidx> <v>     single-channel ledger-anchor probe
 *                   B                      reprint the boot banner (receipt)
 *   board → host:  R <id> <s1..s6> <gray> <dissent> <pen> <verdict> <us>
 *                   Q <id> <chidx> <sev> <gray> <dissent> <us>
 *                   D <id> <chidx> <value> <edge> <dist>      (dissent detail)
 *                   E <text>                                (bad input)
 * severity 0 ok / 1 warn (gray) / 2 bad; verdict 0 accept / 1 revise.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"

extern "C" {
#include "critic_gate.h"
}

#define REFLEX_VERSION "reflex-arc v1"

static char linebuf[128];
static size_t linelen = 0;

static void print_banner()
{
    Serial.println(REFLEX_VERSION " — the critic's frozen gate on ESP32-S3 — no cloud, no model, no radio");
    Serial.printf("gate: critic-gate.qm from gate/gate-bands.json v%d\n", GATE_QM_VERSION);
    Serial.printf("mint-receipt sha256: %s\n", critic_gate_sha256());
    Serial.println("fixed-point: micro-units (1 unit = 1e-6), integer-only — no floats on target");
    Serial.println("scope: 6-channel band gate + gray zone; voice-leading/tension-curve/arc stay desktop");
    Serial.printf("channels (µ):");
    for (int i = 0; i < GATE_QM_N_CHANNELS; i++)
        Serial.printf(" %s[%ld,%ld]", gate_qm_bands[i].name,
                      (long)gate_qm_bands[i].lo, (long)gate_qm_bands[i].hi);
    Serial.println();
    Serial.println("ready — V <id> <f1..f6 µ> · P <id> <chidx> <µ> · B");
}

static void handle_line(const char *line)
{
    while (*line == ' ') line++;
    if (!*line) return;

    if (line[0] == 'B' && (line[1] == '\0' || line[1] == ' ')) {
        print_banner();
        return;
    }

    if (line[0] == 'V') {
        char *end = nullptr;
        long id = strtol(line + 1, &end, 10);
        int32_t f[GATE_QM_N_CHANNELS];
        for (int i = 0; i < GATE_QM_N_CHANNELS; i++) {
            if (!end || *end != ' ') { Serial.printf("E bad V line: %s\n", line); return; }
            f[i] = (int32_t)strtol(end, &end, 10);
        }
        uint32_t t0 = micros();
        gate_judgment_t j;
        critic_gate_judge(f, &j);
        uint32_t dt = micros() - t0;
        Serial.printf("R %ld %d %d %d %d %d %d %d %d %ld %d %lu\n",
                      id, j.sev[0], j.sev[1], j.sev[2], j.sev[3], j.sev[4], j.sev[5],
                      j.gray, j.dissent, (long)j.penalty_u, (int)j.verdict,
                      (unsigned long)dt);
        for (int i = 0; i < GATE_QM_N_CHANNELS; i++) {
            if (j.dissent & (1u << i)) {
                int32_t lo = gate_qm_bands[i].lo, hi = gate_qm_bands[i].hi;
                int32_t dlo = f[i] - lo; if (dlo < 0) dlo = -dlo;
                int32_t dhi = f[i] - hi; if (dhi < 0) dhi = -dhi;
                int32_t edge = dlo <= dhi ? lo : hi;
                Serial.printf("D %ld %d %ld %ld %ld\n", id, i, (long)f[i], (long)edge,
                              (long)(dlo <= dhi ? dlo : dhi));
            }
        }
        return;
    }

    if (line[0] == 'P') {
        char *end = nullptr;
        long id = strtol(line + 1, &end, 10);
        long ch = end ? strtol(end, &end, 10) : -1;
        int32_t v = end ? (int32_t)strtol(end, &end, 10) : 0;
        if (!end || ch < 0 || ch >= GATE_QM_N_CHANNELS) { Serial.printf("E bad P line: %s\n", line); return; }
        uint32_t t0 = micros();
        int32_t sev; int gray, dissent;
        critic_gate_judge_channel((int)ch, v, &sev, &gray, &dissent);
        uint32_t dt = micros() - t0;
        Serial.printf("Q %ld %ld %ld %d %d %lu\n", id, ch, (long)sev, gray, dissent, (unsigned long)dt);
        return;
    }

    Serial.printf("E unknown: %s\n", line);
}

void setup()
{
    Serial.begin(115200);
    /* radio dark — the gate judges alone; nothing leaves the board */
    WiFi.mode(WIFI_OFF);
    btStop();
    delay(50);
    print_banner();
    Serial.println("radio: WiFi off, BT stopped — dark");
}

void loop()
{
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            linebuf[linelen] = '\0';
            handle_line(linebuf);
            linelen = 0;
        } else if (linelen + 1 < sizeof(linebuf)) {
            linebuf[linelen++] = c;
        }
    }
    delay(1);   /* gentle poll — the judge is interrupt-free integer math */
}
