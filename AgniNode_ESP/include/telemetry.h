#pragma once
#include <Arduino.h>

namespace Telemetry_ {
void begin();
void service(); /* alerts, event-driven refresh, 10 s live + history */
bool lastUploadOk();
void setOtaFlag(const char *flag); /* shown in status/live (e.g. "STM32_FLASH_FAILED") */
} // namespace Telemetry_
