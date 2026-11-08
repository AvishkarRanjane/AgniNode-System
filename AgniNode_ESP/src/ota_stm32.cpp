#include "ota_stm32.h"
#include "config.h"
#include "stm_link.h"
#include "telemetry.h"
#include "util.h"
#include <LittleFS.h>

namespace OtaStm {

#define BL_ACK 0x79
#define BL_NACK 0x1F

static HardwareSerial &S = Serial2;

void begin() {
    pinMode(STM_BOOT0_PIN, OUTPUT);
    digitalWrite(STM_BOOT0_PIN, LOW);
    pinMode(STM_NRST_PIN, INPUT); /* released: STM32 pulls NRST up itself */
}

static void stmReset(bool bootloader) {
    digitalWrite(STM_BOOT0_PIN, bootloader ? HIGH : LOW);
    pinMode(STM_NRST_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(STM_NRST_PIN, LOW);
    delay(30);
    digitalWrite(STM_NRST_PIN, HIGH);
    delay(bootloader ? 150 : 20);
    pinMode(STM_NRST_PIN, INPUT);
}

/* ------------------------------------------------------------ AN3155 primitives */
static void flushRx() {
    while (S.available())
        S.read();
}

static bool waitAck(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (S.available()) {
            int b = S.read();
            if (b == BL_ACK)
                return true;
            if (b == BL_NACK)
                return false;
        }
        Util::feedWdt();
        delay(1);
    }
    return false;
}

static bool cmd(uint8_t c, uint32_t timeoutMs = 1000) {
    flushRx();
    uint8_t b[2] = {c, (uint8_t)~c};
    S.write(b, 2);
    S.flush();
    return waitAck(timeoutMs);
}

static bool sendAddr(uint32_t a) {
    uint8_t b[5] = {(uint8_t)(a >> 24), (uint8_t)(a >> 16), (uint8_t)(a >> 8), (uint8_t)a, 0};
    b[4] = b[0] ^ b[1] ^ b[2] ^ b[3];
    S.write(b, 5);
    S.flush();
    return waitAck(1000);
}

static bool sync() {
    Stm::setBootloaderMode(true); /* 115200 8E1, frame parser off */
    for (int i = 0; i < 3; i++) {
        stmReset(true);
        flushRx();
        S.write((uint8_t)0x7F); /* auto-baud */
        S.flush();
        if (waitAck(1000))
            return true;
    }
    return false;
}

static bool eraseAll() /* Extended Erase, global mass erase */
{
    if (!cmd(0x44, 1000))
        return false;
    const uint8_t arg[3] = {0xFF, 0xFF, 0x00};
    S.write(arg, 3);
    S.flush();
    return waitAck(40000);
}

static bool writeBlock(uint32_t addr, const uint8_t *d, size_t n) {
    if (!cmd(0x31) || !sendAddr(addr))
        return false;
    uint8_t x = (uint8_t)(n - 1);
    uint8_t hdr = x;
    for (size_t i = 0; i < n; i++)
        x ^= d[i];
    S.write(&hdr, 1);
    S.write(d, n);
    S.write(&x, 1);
    S.flush();
    return waitAck(3000);
}

static bool readBlock(uint32_t addr, uint8_t *d, size_t n) {
    if (!cmd(0x11) || !sendAddr(addr))
        return false;
    uint8_t a[2] = {(uint8_t)(n - 1), (uint8_t)~(n - 1)};
    S.write(a, 2);
    S.flush();
    if (!waitAck(1000))
        return false;
    size_t got = 0;
    uint32_t t0 = millis();
    while (got < n && millis() - t0 < 2000) {
        if (S.available())
            d[got++] = (uint8_t)S.read();
        else
            delay(1);
    }
    return got == n;
}

/* ------------------------------------------------------------ image handling */
static bool download(const OtaMeta &m, uint32_t &size, String &err) {
    HTTPClient http;
    int len = 0;
    if (!OtaCommon::openStream(http, m.url, len, err))
        return false;
    if (len < 1024 || (uint32_t)len > STM32_FLASH_SIZE) {
        err = "BAD_IMAGE_SIZE";
        http.end();
        return false;
    }

    LittleFS.remove(STM_IMAGE_PATH);
    File f = LittleFS.open(STM_IMAGE_PATH, FILE_WRITE);
    if (!f) {
        err = "FS_OPEN_FAILED";
        http.end();
        return false;
    }

    OtaCommon::Sha256 sha;
    sha.begin();
    WiFiClient *s = http.getStreamPtr();
    uint8_t buf[1024];
    size_t total = 0;
    uint32_t lastData = millis();

    while (total < (size_t)len) {
        size_t avail = s->available();
        if (avail) {
            int n = s->readBytes(buf, min(avail, sizeof(buf)));
            if (n <= 0)
                continue;
            if (f.write(buf, (size_t)n) != (size_t)n) {
                err = "FS_WRITE_FAILED";
                f.close();
                http.end();
                return false;
            }
            sha.update(buf, (size_t)n);
            total += (size_t)n;
            lastData = millis();
        } else {
            if (!http.connected() || millis() - lastData > 20000UL) {
                err = "DOWNLOAD_STALLED";
                f.close();
                http.end();
                return false;
            }
            delay(2);
        }
        Util::feedWdt();
    }
    f.close();
    http.end();

    uint8_t hash[32];
    sha.finish(hash);
    if (!OtaCommon::verify(hash, m, err)) {
        LittleFS.remove(STM_IMAGE_PATH);
        return false;
    }

    /* sanity: initial SP must be in SRAM, reset vector must be a Thumb address inside flash */
    File r = LittleFS.open(STM_IMAGE_PATH, FILE_READ);
    uint8_t h[8] = {0};
    r.read(h, 8);
    r.close();
    uint32_t sp = h[0] | (h[1] << 8) | (h[2] << 16) | ((uint32_t)h[3] << 24);
    uint32_t rv = h[4] | (h[5] << 8) | (h[6] << 16) | ((uint32_t)h[7] << 24);
    if (sp < 0x20000000UL || sp > STM32_RAM_TOP || !(rv & 1U) || rv < STM32_FLASH_BASE ||
        rv > STM32_FLASH_BASE + STM32_FLASH_SIZE) {
        LittleFS.remove(STM_IMAGE_PATH);
        err = "BAD_VECTOR_TABLE";
        return false;
    }
    size = (uint32_t)total;
    return true;
}

static bool flashOnce(uint32_t size, String &err) {
    if (!sync()) {
        err = "BOOTLOADER_NO_RESPONSE";
        return false;
    }
    if (!eraseAll()) {
        err = "ERASE_FAILED";
        return false;
    }

    File f = LittleFS.open(STM_IMAGE_PATH, FILE_READ);
    if (!f) {
        err = "FS_OPEN_FAILED";
        return false;
    }

    uint8_t buf[256];
    for (uint32_t off = 0; off < size; off += 256) {
        size_t n = min((uint32_t)256, size - off);
        if (f.read(buf, n) != n) {
            err = "FS_READ_FAILED";
            f.close();
            return false;
        }
        size_t padded = (n + 7U) & ~7U; /* flash programs 64-bit words */
        for (size_t i = n; i < padded; i++)
            buf[i] = 0xFF;
        if (!writeBlock(STM32_FLASH_BASE + off, buf, padded)) {
            err = "WRITE_FAILED";
            f.close();
            return false;
        }
        Util::feedWdt();
    }

    /* read everything back and compare */
    f.seek(0);
    uint8_t rb[256];
    for (uint32_t off = 0; off < size; off += 256) {
        size_t n = min((uint32_t)256, size - off);
        f.read(buf, n);
        if (!readBlock(STM32_FLASH_BASE + off, rb, n) || memcmp(buf, rb, n) != 0) {
            err = "VERIFY_FAILED";
            f.close();
            return false;
        }
        Util::feedWdt();
    }
    f.close();
    return true;
}

bool run(const OtaMeta &m, String &err) {
    if (!LittleFS.begin(true)) {
        err = "FS_MOUNT_FAILED";
        return false;
    }
    Telemetry &t = Stm::tel();

    /* safety gate: never flash while the STM32 is protecting a live hazard */
    if (Stm::online()) {
        if (strcmp(t.state, "NORMAL") != 0) {
            err = "UNSAFE_STATE";
            return false;
        }
    } else if (!m.force) {
        err = "STM32_OFFLINE"; /* offline = already unprotected: needs force=true */
        return false;
    }

    uint32_t size = 0;
    if (!download(m, size, err))
        return false;

    if (Stm::online()) {
        AckInfo a;
        if (!Stm::execBlocking("PREPOTA", a, 1500, 3)) {
            err = "PREPARE_NO_ACK";
            return false;
        }
        if (strcmp(a.result, "OK") != 0) {
            err = String("PREPARE_") + a.detail;
            return false;
        }
    }

    Telemetry_::setOtaFlag("STM32_FLASHING");
    String lastErr;
    bool flashed = false;
    for (int attempt = 1; attempt <= STM_OTA_ATTEMPTS && !flashed; attempt++) {
        Serial.printf("[OTA-STM] flash attempt %d/%d (%lu bytes)\n", attempt, STM_OTA_ATTEMPTS,
                      (unsigned long)size);
        flashed = flashOnce(size, lastErr);
    }

    Stm::setBootloaderMode(false); /* back to 8N1 before the app boots */
    stmReset(false);

    if (!flashed) {
        err = "FLASH_FAILED_" + lastErr;
        Telemetry_::setOtaFlag(
            "STM32_FLASH_FAILED"); /* image stays in LittleFS for a retry command */
        return false;
    }

    uint32_t t0 = millis();
    bool booted = false;
    while (millis() - t0 < 15000UL) {
        Stm::poll();
        if (Stm::takeBoot()) {
            booted = true;
            break;
        }
        Util::feedWdt();
        delay(5);
    }
    Telemetry_::setOtaFlag("");
    if (!booted) {
        err = "NO_BOOT_AFTER_FLASH";
        return false;
    }
    if (m.version.length() && m.version != Stm::tel().fw) {
        err = "VERSION_MISMATCH";
        return false;
    }

    Stm::markLiveDirty();
    return true;
}

} // namespace OtaStm
