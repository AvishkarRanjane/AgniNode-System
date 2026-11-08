#pragma once
#include "ota_common.h"

/* Flashes the STM32G0 through its built-in UART bootloader (AN3155, USART1 on PA9/PA10):
 * ESP32 drives BOOT0 + NRST, so recovery works even if the STM32 application is dead. */
namespace OtaStm {
void begin(); /* BOOT0 low, NRST released */
bool run(const OtaMeta &m, String &err);
} // namespace OtaStm
