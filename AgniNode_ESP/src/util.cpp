#include "util.h"
#include <esp_task_wdt.h>

namespace Util {

uint8_t xorChecksum(const char *s, size_t n) {
    uint8_t x = 0;
    for (size_t i = 0; i < n; i++)
        x ^= (uint8_t)s[i];
    return x;
}

bool timeValid() {
    return time(nullptr) > 1700000000;
}

String isoTime(time_t t) {
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char b[24];
    strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return String(b);
}

time_t isoToEpoch(const char *s) {
    int Y, M, D, h, m, sec;
    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) != 6)
        return 0;
    Y -= (M <= 2);
    int era = (Y >= 0 ? Y : Y - 399) / 400;
    unsigned yoe = (unsigned)(Y - era * 400);
    unsigned doy = (unsigned)((153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = (long)era * 146097L + (long)doe - 719468L;
    return (time_t)(days * 86400L + h * 3600L + m * 60L + sec);
}

bool validDocId(const String &id) {
    if (id.length() == 0 || id.length() > 64)
        return false;
    for (size_t i = 0; i < id.length(); i++) {
        char c = id[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

bool hexToBytes(const char *hex, uint8_t *out, size_t n) {
    if (!hex || strlen(hex) != n * 2)
        return false;
    for (size_t i = 0; i < n; i++) {
        char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
        if (!isxdigit((unsigned char)b[0]) || !isxdigit((unsigned char)b[1]))
            return false;
        out[i] = (uint8_t)strtoul(b, nullptr, 16);
    }
    return true;
}

void feedWdt() {
    esp_task_wdt_reset();
}

} // namespace Util
