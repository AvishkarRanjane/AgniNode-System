#include "stm32g0xx_hal.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}
