#include "ota_esp32.h"
#include "config.h"
#include "net.h"
#include "stm_link.h"
#include "util.h"
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>

namespace OtaEsp32 {

void bootCheck() {
    Preferences p;
    p.begin("otaesp", false);
    String doc = p.isKey("doc") ? p.getString("doc") : "";
    if (doc.length() && !p.getBool("rb", false)) {
        uint8_t boots = p.getUChar("boots", 0) + 1;
        p.putUChar("boots", boots);
        if (boots > OTA_MAX_BOOT_ATTEMPTS) {
            String prev = p.isKey("prev") ? p.getString("prev") : "";
            const esp_partition_t *part = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, prev.c_str());
            if (part && esp_ota_set_boot_partition(part) == ESP_OK) {
                p.putBool("rb", true);
                p.end();
                Serial.println("[OTA] new firmware never confirmed -> rolling back");
                delay(200);
                ESP.restart();
            }
        }
    }
    p.end();
}

bool pendingConfirmation(String &docId) {
    Preferences p;
    p.begin("otaesp", true);
    docId = p.isKey("doc") ? p.getString("doc") : "";
    bool pending = docId.length() && !p.getBool("rb", false);
    p.end();
    return pending;
}

void confirm() {
    Preferences p;
    p.begin("otaesp", false);
    p.clear();
    p.end();
}

bool takeRollbackReport(String &docId) {
    Preferences p;
    p.begin("otaesp", false);
    bool rb = p.getBool("rb", false);
    docId = p.isKey("doc") ? p.getString("doc") : "";
    if (rb && docId.length()) {
        p.clear();
        p.end();
        return true;
    }
    p.end();
    return false;
}

bool run(const OtaMeta &m, const String &docId, String &err) {
    if (Stm::online() && strcmp(Stm::tel().state, "NORMAL") != 0 && !m.force) {
        err = "UNSAFE_STATE";
        return false;
    }

    HTTPClient http;
    int len = 0;
    if (!OtaCommon::openStream(http, m.url, len, err))
        return false;

    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    if (!next || (size_t)len > next->size) {
        err = "IMAGE_TOO_LARGE";
        http.end();
        return false;
    }
    if (!Update.begin((size_t)len, U_FLASH)) {
        err = "UPDATE_BEGIN_FAILED";
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
            if (Update.write(buf, (size_t)n) != (size_t)n) {
                err = "FLASH_WRITE_FAILED";
                Update.abort();
                http.end();
                return false;
            }
            sha.update(buf, (size_t)n);
            total += (size_t)n;
            lastData = millis();
        } else {
            if (!http.connected() || millis() - lastData > 20000UL) {
                err = "DOWNLOAD_STALLED";
                Update.abort();
                http.end();
                return false;
            }
            delay(2);
        }
        Util::feedWdt();
    }
    http.end();

    uint8_t hash[32];
    sha.finish(hash);
    if (!OtaCommon::verify(hash, m, err)) {
        Update.abort();
        return false;
    } /* boot slot still untouched */

    /* remember how to roll back, then commit */
    Preferences p;
    p.begin("otaesp", false);
    p.clear();
    p.putString("doc", docId);
    p.putString("prev", esp_ota_get_running_partition()->label);
    p.putUChar("boots", 0);
    p.end();

    if (!Update.end(true)) {
        err = "UPDATE_END_FAILED";
        confirm();
        return false;
    }

    Serial.println("[OTA] ESP32 image verified and committed, restarting");
    Serial.flush();
    delay(500);
    ESP.restart();
    return true;
}

} // namespace OtaEsp32
