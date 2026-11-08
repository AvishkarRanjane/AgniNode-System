/*
 * AgniNode gateway (ESP32-WROOM-32E)
 *   STM32 UART  <->  Wi-Fi / TLS  <->  Firebase Auth + Firestore
 * The STM32 stays the safety authority; this gateway reports, relays commands and updates firmware.
 */
#include "auth.h"
#include "commands.h"
#include "config.h"
#include "net.h"
#include "ota_esp32.h"
#include "stm_link.h"
#include "telemetry.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.printf("\n[BOOT] ESP32 Initialized. Waiting for STM32 handshake... "
                  "(reset_reason=%d, serial_monitor_baud=115200)\n",
                  (int)esp_reset_reason());
    Serial.flush();

    Serial.println("[Boot] checking OTA state before starting services");
    Serial.flush();
    OtaEsp32::bootCheck(); /* rollback logic runs before anything else */
    Serial.println("[Boot] OTA check complete");

    esp_task_wdt_init(TASK_WDT_SECONDS, true);
    esp_task_wdt_add(nullptr);

    if (!LittleFS.begin(true))
        Serial.println("[Boot] LittleFS mount failed");
    else
        Serial.println("[Boot] LittleFS mounted");
    Stm::begin();
    Serial.println("[Boot] STM UART started");
    Serial.println(
        "[UART] Direct link uses STM32 PA9/PA10; PB6/PA3/PA8 RS-485 remains a separate path");
    Net::begin();
    Serial.println("[Boot] Wi-Fi connection requested");
    Auth::begin();
    Serial.println(
        "[Boot] Firebase Auth initialized; token acquisition deferred until cloud is ready");
    Telemetry_::begin();
    Commands::begin();
    Serial.println("[Boot] setup complete");
}

void loop() {
    static bool cloudReadyReported = false;
    static bool heartbeatReported = false;
    static uint32_t lastHeartbeatMs = 0;
    esp_task_wdt_reset();
    Stm::poll();
    Net::service();

    bool cloudReady = Net::online();
    if (cloudReady && !cloudReadyReported) {
        Serial.println("[Cloud] Wi-Fi and clock ready; enabling Firestore services");
        cloudReadyReported = true;
    } else if (!cloudReady && cloudReadyReported) {
        Serial.println("[Cloud] Wi-Fi or clock unavailable; pausing Firestore services");
        cloudReadyReported = false;
    }

    if (cloudReady) {
        Commands::service();
        Telemetry_::service();
    }

    uint32_t now = millis();
    if (!heartbeatReported || now - lastHeartbeatMs >= SERIAL_HEARTBEAT_MS) {
        lastHeartbeatMs = now;
        heartbeatReported = true;
        bool wifiConnected = Net::wifiConnected();
        bool gatewayOnline = Net::online();
        String ip = wifiConnected ? WiFi.localIP().toString() : String("-");
        Serial.printf("[Heartbeat] uptime=%lus heap=%lu gatewayOnline=%s wifi=%s(status=%d,ip=%s) "
                      "ntp=%s cloudServices=%s stm32=%s\n",
                      (unsigned long)(now / 1000UL), (unsigned long)ESP.getFreeHeap(),
                      gatewayOnline ? "true" : "false",
                      wifiConnected ? "connected" : "disconnected", Net::wifiStatus(), ip.c_str(),
                      Net::clockValid() ? "valid" : "waiting", cloudReady ? "enabled" : "gated",
                      Stm::online() ? "online" : "waiting/offline");
    }
    delay(5);
}
