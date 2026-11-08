/*
 * Standalone STM32G0B1RE test: drive the active-high buzzer output on PB12.
 * Use only with the buzzer circuit connected to PB12 and other load outputs
 * safely disconnected.
 */
#include "stm32g0xx_hal.h"

int main() {
    HAL_Init();

    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* Turn buzzer ON */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

    while (true) {
        HAL_Delay(500);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
    }
}
