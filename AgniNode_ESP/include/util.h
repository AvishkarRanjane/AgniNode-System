#pragma once
#include <Arduino.h>
#include <time.h>

namespace Util {
uint8_t xorChecksum(const char *s, size_t n);
bool timeValid();
String isoTime(time_t t);
time_t isoToEpoch(const char *s); /* "2026-10-25T10:00:00.123Z" -> epoch (UTC) */
bool validDocId(const String &id);
bool hexToBytes(const char *hex, uint8_t *out, size_t n);
void feedWdt();
} // namespace Util
