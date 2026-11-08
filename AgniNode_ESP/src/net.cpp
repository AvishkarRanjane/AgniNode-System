#include "net.h"
#include "config.h"
#include "secrets.h"
#include "util.h"
#include <WiFi.h>

/* certs/roots.pem is embedded by PlatformIO (board_build.embed_txtfiles) and NUL-terminated */
extern const char roots_pem[] asm("_binary_certs_roots_pem_start");

namespace Net {

static uint32_t lastTryMs = 0;
static bool connectionReported = false;
static bool timeReported = false;
static bool timeWaitReported = false;
static uint32_t lastTimeWaitLogMs = 0;
static uint32_t reconnectAttempts = 0;
static uint32_t wifiStartMs = 0;
static volatile bool disconnectEventPending = false;
static volatile uint8_t lastDisconnectReason = 0;

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        lastDisconnectReason = info.wifi_sta_disconnected.reason;
        disconnectEventPending = true;
    }
}

static const char *wifiStatusName(wl_status_t status) {
    switch (status) {
    case WL_IDLE_STATUS:
        return "idle";
    case WL_NO_SSID_AVAIL:
        return "ssid-not-found";
    case WL_SCAN_COMPLETED:
        return "scan-complete";
    case WL_CONNECTED:
        return "connected";
    case WL_CONNECT_FAILED:
        return "connect-failed";
    case WL_CONNECTION_LOST:
        return "connection-lost";
    case WL_DISCONNECTED:
        return "disconnected";
    case WL_NO_SHIELD:
        return "no-shield";
    default:
        return "unknown";
    }
}

void begin() {
    wifiStartMs = millis();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastTryMs = millis();
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    Serial.printf("[WiFi] connecting to configured SSID; retry=%lu ms\n",
                  (unsigned long)WIFI_RETRY_MS);
    Serial.println("[Time] NTP sync requested (pool.ntp.org, time.google.com)");
}

void service() {
    if (disconnectEventPending) {
        uint8_t reason = lastDisconnectReason;
        disconnectEventPending = false;
        Serial.printf("[WiFi] station disconnect event; reason=%u\n", (unsigned)reason);
    }

    wl_status_t status = WiFi.status();
    bool connected = status == WL_CONNECTED;
    if (connected && !connectionReported) {
        Serial.printf("[WIFI] Connected to Network (IP: %s, RSSI: %d dBm, after %lu ms)\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                      (unsigned long)(millis() - wifiStartMs));
        connectionReported = true;
    } else if (!connected && connectionReported) {
        Serial.printf("[WiFi] disconnected (status=%d: %s)\n", (int)status, wifiStatusName(status));
        connectionReported = false;
        timeWaitReported = false;
    }

    bool clockValid = Util::timeValid();
    if (clockValid && !timeReported) {
        Serial.printf("[Time] clock synchronized; epoch=%lu\n", (unsigned long)time(nullptr));
        timeReported = true;
        timeWaitReported = false;
    } else if (!clockValid) {
        timeReported = false;
        if (connected && (!timeWaitReported || millis() - lastTimeWaitLogMs >= 30000UL)) {
            Serial.printf("[Time] waiting for NTP for %lu ms; Firestore services remain gated\n",
                          (unsigned long)(millis() - wifiStartMs));
            timeWaitReported = true;
            lastTimeWaitLogMs = millis();
        }
    }

    if (connected || millis() - lastTryMs < WIFI_RETRY_MS)
        return;
    lastTryMs = millis();
    reconnectAttempts++;
    Serial.printf("[WiFi] attempt %lu failed/pending after %lu ms (status=%d: %s); retrying\n",
                  (unsigned long)reconnectAttempts, (unsigned long)(millis() - wifiStartMs),
                  (int)status, wifiStatusName(status));
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool online() {
    return WiFi.status() == WL_CONNECTED && Util::timeValid();
}
bool wifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}
bool clockValid() {
    return Util::timeValid();
}
int wifiStatus() {
    return (int)WiFi.status();
}

WiFiClientSecure &tls() {
    static WiFiClientSecure client;
    static bool ready = false;
    if (!ready) {
        client.setCACert(roots_pem); /* full chain + hostname validation */
        client.setHandshakeTimeout(30);
        ready = true;
    }
    return client;
}

} // namespace Net
