/* comms.c - UART JSON telemetry transmitter & command receiver for ESP32 gateway */
#include "comms.h"
#include "board.h"
#include "config.h"
#include "protection.h"
#include "sensors.h"
#include <stdio.h>
#include <string.h>

static uint32_t last_telemetry_ms = 0;
static bool prev_fire = false;
static bool prev_arc = false;
static bool prev_shock = false;
static bool prev_trip = false;

static char rx_buf[128];
static uint8_t rx_idx = 0;

void comms_init(void) {
    last_telemetry_ms = 0;
    prev_fire = false;
    prev_arc = false;
    prev_shock = false;
    prev_trip = false;
    rx_idx = 0;
}

static void send_telemetry_packet(const sensors_t *s, const protection_state_t *p) {
    char pkt[220];

    int len = snprintf(
        pkt, sizeof(pkt),
        "{\"v_rms\":%.1f,\"smoke_analog\":%.2f,\"smoke_dig\":%d,\"temp\":%.1f,\"flame\":%d,"
        "\"fire\":%s,\"arc_fault\":%s,\"shock\":%s,\"ssr_tripped\":%s}\n",
        (double)s->v_rms, (double)s->smoke_analog_v, (int)s->smoke_dig, (double)s->temp_c,
        (int)s->flame_dig, p->fire ? "true" : "false", p->arc_fault ? "true" : "false",
        p->shock ? "true" : "false", p->ssr_tripped ? "true" : "false");

    if (len > 0) {
        board_uart_send((const uint8_t *)pkt, (uint16_t)len);
    }
}

void comms_poll(uint32_t now_ms) {
    uint8_t ch = 0;
    while (board_uart_rx_pop(&ch)) {
        if (ch == '\n' || ch == '\r') {
            if (rx_idx > 0) {
                rx_buf[rx_idx] = '\0';

                /* Process remote commands from ESP32 */
                if (strstr(rx_buf, "RESET") != NULL) {
                    protection_reset(now_ms);
                } else if (strstr(rx_buf, "TRIP") != NULL) {
                    protection_manual_trip(now_ms);
                } else if (strstr(rx_buf, "BUZZER_OFF") != NULL || strstr(rx_buf, "MUTE") != NULL) {
                    protection_buzzer_override(false);
                } else if (strstr(rx_buf, "BUZZER_ON") != NULL) {
                    protection_buzzer_override(true);
                }

                rx_idx = 0;
            }
        } else if (rx_idx < (sizeof(rx_buf) - 1)) {
            rx_buf[rx_idx++] = (char)ch;
        }
    }
}

void comms_periodic(uint32_t now_ms) {
    const sensors_t *s = sensors_get();
    const protection_state_t *p = protection_get_state();

    /* Trigger immediate transmission on any fault state change */
    bool state_changed = (p->fire != prev_fire) || (p->arc_fault != prev_arc) ||
                         (p->shock != prev_shock) || (p->ssr_tripped != prev_trip);

    if (state_changed || (int32_t)(now_ms - last_telemetry_ms) >= (int32_t)TELEMETRY_PERIOD_MS) {
        last_telemetry_ms = now_ms;
        prev_fire = p->fire;
        prev_arc = p->arc_fault;
        prev_shock = p->shock;
        prev_trip = p->ssr_tripped;

        send_telemetry_packet(s, p);
    }
}
