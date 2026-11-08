#include "actuators.h"
#include "config.h"
#include "stm32g0xx_hal.h"

static bool buzzer_state = false;
static bool ssr_tripped_state = false;

void actuators_init(void) {
    /* Clocks enabled in board_init */
    buzzer_state = false;
    ssr_tripped_state = false;

    /* Buzzer off */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    /* SSR untripped (High-Z open drain) */
    HAL_GPIO_WritePin(SSR_PORT, SSR_PIN, GPIO_PIN_SET);
}

void actuator_set_buzzer(bool on) {
    buzzer_state = on;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void actuator_set_ssr(bool tripped) {
    ssr_tripped_state = tripped;
    /* In Open-Drain mode (GPIO_MODE_OUTPUT_OD):
     * RESET = Pin pulls to GND (sinks current to trip/de-energize breaker)
     * SET   = Pin is High-Impedance (normal energized state)
     */
    HAL_GPIO_WritePin(SSR_PORT, SSR_PIN, tripped ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool actuator_buzzer_is_on(void) {
    return buzzer_state;
}

bool actuator_ssr_is_tripped(void) {
    return ssr_tripped_state;
}
