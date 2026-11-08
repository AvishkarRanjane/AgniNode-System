#include "stm_link.h"
#include "config.h"
#include <ArduinoJson.h>

namespace Stm {

static Telemetry T;
static Analysis AN;
static AckInfo A;
static bool liveDirty = false;
static bool bootSeen = false;
static bool bootMode = false;
static bool ackReady = false;
static bool ackBad = false;
static uint32_t expectSeqNum = 0;
static char lineBuf[256];
static size_t lineLen = 0;

HardwareSerial &port() {
    return Serial2;
}

Telemetry &tel() {
    return T;
}

Analysis &ana() {
    return AN;
}

bool takeAnalysisSample() {
    return false;
}

bool online() {
    return T.everReceived && (millis() - T.lastRxMs) < STM_TIMEOUT_MS;
}

bool takeLiveDirty() {
    bool d = liveDirty;
    liveDirty = false;
    return d;
}

void markLiveDirty() {
    liveDirty = true;
}

bool takeBoot() {
    bool b = bootSeen;
    bootSeen = false;
    return b;
}

bool takeAlert(char *out, size_t n) {
    (void)out;
    (void)n;
    return false;
}

uint32_t nextSeq() {
    static uint32_t s = 1;
    return s++;
}

void buildFrame(uint32_t seq, const char *body, char *out, size_t n) {
    snprintf(out, n, "{\"seq\":%lu,\"cmd\":\"%s\"}\n", (unsigned long)seq, body ? body : "");
}

void sendFrame(const char *frame) {
    if (frame) {
        Serial2.print(frame);
    }
}

void sendCommand(const char *cmd) {
    if (cmd) {
        Serial2.println(cmd);
    }
}

void expectAck(uint32_t seq) {
    expectSeqNum = seq;
    ackReady = false;
    ackBad = false;
}

bool takeAck(AckInfo &out) {
    if (!ackReady)
        return false;
    out = A;
    ackReady = false;
    return true;
}

bool takeAckBad() {
    bool b = ackBad;
    ackBad = false;
    return b;
}

bool execBlocking(const char *body, AckInfo &out, uint32_t perAttemptMs, uint8_t attempts) {
    (void)body;
    (void)out;
    (void)perAttemptMs;
    (void)attempts;
    return true;
}

void setBootloaderMode(bool on) {
    bootMode = on;
    Serial2.end();
    Serial2.setRxBufferSize(1024);
    Serial2.begin(STM_UART_BAUD, on ? SERIAL_8E1 : SERIAL_8N1, STM_UART_RX_PIN, STM_UART_TX_PIN);
    while (Serial2.available())
        Serial2.read();
    lineLen = 0;
    if (!on) {
        T.everReceived = false;
        bootSeen = false;
    }
}

static void parseJsonTelemetry(const char *jsonStr) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.printf("[STM UART] JSON Parse Error: %s\n", err.c_str());
        return;
    }

    /* Extract telemetry fields */
    T.v_rms = doc["v_rms"] | 0.0f;
    T.smoke_analog = doc["smoke_analog"] | 0.0f;
    T.smoke_dig = doc["smoke_dig"] | 0;
    T.temp = doc["temp"] | 0.0f;
    T.flame = doc["flame"] | 0;
    T.fire = doc["fire"] | false;
    T.arc_fault = doc["arc_fault"] | false;
    T.shock = doc["shock"] | false;
    T.ssr_tripped = doc["ssr_tripped"] | false;

    /* Populate compatibility fields */
    T.temperature = T.temp;
    T.tempValid = 1;
    T.smoke = (int32_t)(T.smoke_analog * 1000.0f);
    T.d0 = T.smoke_dig;
    T.buz = (T.fire || T.arc_fault || T.shock) ? 1 : 0;
    T.trip = (T.arc_fault ? 0x02 : 0) | (T.shock ? 0x01 : 0) | (T.fire ? 0x04 : 0);
    T.mains = T.ssr_tripped ? 0 : 1;

    if (T.fire)
        strlcpy(T.state, "FIRE", sizeof(T.state));
    else if (T.arc_fault)
        strlcpy(T.state, "ARC_FAULT", sizeof(T.state));
    else if (T.shock)
        strlcpy(T.state, "SHOCK_FAULT", sizeof(T.state));
    else
        strlcpy(T.state, "NORMAL", sizeof(T.state));

    strlcpy(T.fw, "3.2.0", sizeof(T.fw));
    strlcpy(T.resetCause, "RUN", sizeof(T.resetCause));

    T.everReceived = true;
    T.lastRxMs = millis();

    AN.vrms = T.v_rms;
    AN.valid = true;
    AN.lastMs = T.lastRxMs;

    liveDirty = true;
}

void begin() {
    memset(&T, 0, sizeof(T));
    strlcpy(T.state, "UNKNOWN", sizeof(T.state));
    Serial2.setRxBufferSize(1024);
    Serial2.begin(STM_UART_BAUD, SERIAL_8N1, STM_UART_RX_PIN, STM_UART_TX_PIN);
    lineLen = 0;
}

void poll() {
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        if (c == '\n' || c == '\r') {
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                if (lineBuf[0] == '{') {
                    parseJsonTelemetry(lineBuf);
                }
                lineLen = 0;
            }
        } else if (lineLen < (sizeof(lineBuf) - 1)) {
            lineBuf[lineLen++] = c;
        }
    }
}

} // namespace Stm
