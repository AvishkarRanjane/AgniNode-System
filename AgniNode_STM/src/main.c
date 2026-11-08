/*
 * AgniNode Safety MCU - STM32G0B1RE @ 64 MHz
 * Complete physical hardware implementation matching pinout:
 *   - PA0: ZMPT101B AC voltage sensor (via 220/100 ohm divider, 3.2x multiplier)
 *   - PA1: MQ-2 Smoke sensor AO (via 220/100 ohm divider, 3.2x multiplier)
 *   - PB0: MQ-2 Smoke sensor DO (GPIO_NOPULL, active LOW)
 *   - PA8: Temperature sensor DATA/OUT (3.3V)
 *   - PB14: Flame sensor DO/OUT (3.3V, active LOW)
 *   - PB12: Buzzer module I/O (3.3V, active HIGH)
 *   - PB13: Solid State Relay CH1/IN (5V Open-Drain)
 *   - PA9 / PA10: USART1 115200 Baud bridge to ESP32 companion gateway
 */
#include "actuators.h"
#include "analysis.h"
#include "board.h"
#include "comms.h"
#include "config.h"
#include "fire_logic.h"
#include "protection.h"
#include "sensors.h"

int main(void) {
    /* Hardware Peripherals & Core Clock */
    board_init();
    actuators_init();
    analysis_init();
    sensors_init();
    fire_init();
    protection_init();
    comms_init();

    uint32_t next_slow = HAL_GetTick();

    for (;;) {
        uint32_t now = HAL_GetTick();

        /* Fast path: Process 20ms AC frame buffers as soon as DMA fills them */
        for (int budget = 4; budget > 0 && analysis_poll(); budget--) {
            protection_on_frame(analysis_last(), now);
        }

        /* Slow path: 100ms safety decision & periodic loop */
        if ((int32_t)(now - next_slow) >= 0) {
            next_slow += LOOP_PERIOD_MS;
            if ((int32_t)(now - next_slow) > (int32_t)(5U * LOOP_PERIOD_MS))
                next_slow = now + LOOP_PERIOD_MS;

            sensors_update(now);
            comms_poll(now);
            fire_step(sensors_get(), now);
            protection_step(sensors_get(), now);
            comms_periodic(now);
        }

        board_wdg_kick();
    }
}
