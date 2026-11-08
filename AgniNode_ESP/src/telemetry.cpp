#include "telemetry.h"
#include "auth.h"
#include "config.h"
#include "firestore.h"
#include "net.h"
#include "secrets.h"
#include "stm_link.h"
#include "util.h"
#include <WiFi.h>
#include <esp_ota_ops.h>

namespace Telemetry_ {

static uint32_t lastUploadMs = 0;
static bool uploadOk = false;
static bool firebaseConnectedReported = false;

void begin() {
    lastUploadMs = 0;
}

bool lastUploadOk() {
    return uploadOk;
}

void setOtaFlag(const char *f) {
    (void)f;
}

static String buildBody() {
    JsonDocument doc;
    JsonObject f = doc["fields"].to<JsonObject>();
    Telemetry &t = Stm::tel();

    bool alarm = t.fire || t.arc_fault || t.shock || t.ssr_tripped;
    bool warning = (t.smoke_analog > 1.5f) || (t.temp > 45.0f);
    const char *dashboardStatus = alarm ? "ALARM" : (warning ? "WARNING" : "NORMAL");
    time_t now = time(nullptr);

    /* Required exact hardware fields */
    Firestore::fDouble(f, "v_rms", t.v_rms);
    Firestore::fDouble(f, "smoke_analog", t.smoke_analog);
    Firestore::fInt(f, "smoke_dig", t.smoke_dig);
    Firestore::fDouble(f, "temp", t.temp);
    Firestore::fInt(f, "flame", t.flame);
    Firestore::fBool(f, "fire", t.fire);
    Firestore::fBool(f, "arc_fault", t.arc_fault);
    Firestore::fBool(f, "shock", t.shock);
    Firestore::fBool(f, "ssr_tripped", t.ssr_tripped);
    Firestore::fTime(f, "timestamp", now);

    /* Dashboard & Cloud Identity Fields */
    Firestore::fString(f, "deviceId", DEVICE_ID);
    Firestore::fString(f, "name", DEVICE_ID);
    Firestore::fString(f, "status", dashboardStatus);
    Firestore::fBool(f, "gatewayOnline", Net::online());
    Firestore::fBool(f, "stm32Online", Stm::online());
    Firestore::fString(f, "state", t.everReceived ? t.state : "UNKNOWN");

    /* Backward compatibility mappings for UI telemetry charts */
    Firestore::fDouble(f, "voltageV", t.v_rms);
    Firestore::fDouble(f, "voltage", t.v_rms);
    Firestore::fDouble(f, "temperatureC", t.temp);
    Firestore::fDouble(f, "temperature", t.temp);
    Firestore::fInt(f, "smokeAdc", (int)(t.smoke_analog * 1000.0f));
    Firestore::fInt(f, "smoke", (int)(t.smoke_analog * 1000.0f));
    Firestore::fBool(f, "flameDetected", t.flame == 0);
    Firestore::fBool(f, "fireConfirmed", t.fire);
    Firestore::fBool(f, "suppressionActive", t.ssr_tripped);
    Firestore::fBool(f, "buzzerOn", t.buz != 0);
    Firestore::fBool(f, "mainsClosed", !t.ssr_tripped);
    Firestore::fBool(f, "afddTrip", t.arc_fault);
    Firestore::fBool(f, "rccbTrip", t.shock);
    Firestore::fInt(f, "tripMask",
                    (t.arc_fault ? 0x02 : 0) | (t.shock ? 0x01 : 0) | (t.fire ? 0x04 : 0));
    Firestore::fBool(f, "analysisOnline", Stm::online());

    Firestore::fInt(f, "wifiRssi", WiFi.RSSI());
    Firestore::fInt(f, "uptimeSec", (long)(millis() / 1000UL));
    Firestore::fString(f, "stm32Firmware", "3.2.0");
    Firestore::fString(f, "gatewayFirmware", FW_VERSION_ESP);
    Firestore::fTime(f, "updatedAt", now);

    String body;
    serializeJson(doc, body);
    return body;
}

static bool uploadLive() {
    String base = Firestore::devicePath();
    String body = buildBody();
    int rootCode = Firestore::send("PATCH", base, body);
    int liveCode = Firestore::send("PATCH", base + "/status/live", body);
    bool ok = rootCode >= 200 && rootCode < 300 && liveCode >= 200 && liveCode < 300;

    Serial.printf("[Telemetry] upload %s (root HTTP %d, live HTTP %d, gatewayOnline=%s)\n",
                  ok ? "succeeded" : "failed", rootCode, liveCode,
                  Net::online() ? "true" : "false");

    if (ok && !firebaseConnectedReported) {
        firebaseConnectedReported = true;
        Serial.println("[FIREBASE] Authenticated & Connected.");
    }
    if (ok)
        Serial.println("[FIREBASE TX] Telemetry pushed to cloud -> website synced.");

    uploadOk = ok;
    return ok;
}

void service() {
    if (Stm::takeLiveDirty()) {
        uploadLive();
        lastUploadMs = millis();
    }

    if (millis() - lastUploadMs >= UPLOAD_INTERVAL_MS) {
        lastUploadMs = millis();
        uploadLive();
    }
}

} // namespace Telemetry_
