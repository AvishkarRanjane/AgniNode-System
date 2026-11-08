#pragma once
#include <Arduino.h>

struct Telemetry {
    /* Exact new hardware fields */
    float v_rms;
    float smoke_analog;
    int smoke_dig;
    float temp;
    int flame;
    bool fire;
    bool arc_fault;
    bool shock;
    bool ssr_tripped;

    /* Compatibility fields */
    char state[24];
    char fw[16];
    char resetCause[10];
    char lastAlert[56];
    float temperature;
    float humidity;
    float tempRate;
    float vbus;
    int32_t smoke;
    int d0, buz, arm, tempValid, vbusValid, faults, mains, trip;

    bool everReceived;
    uint32_t lastRxMs;
};

struct Analysis {
    float vrms, il, in, ires, iresMax, p, pf, freq, energy;
    int hf, delta, arc, rthr;
    bool valid;
    uint32_t lastMs;
};

struct AckInfo {
    char result[8];
    char detail[32];
    char state[24];
    int buz, ssr, arm, faults, mains, trip;
};

namespace Stm {
void begin();
void poll();
Telemetry &tel();
Analysis &ana();
bool takeAnalysisSample();
bool online();

bool takeAlert(char *out, size_t n);
bool takeLiveDirty();
void markLiveDirty();
bool takeBoot();

uint32_t nextSeq();
void buildFrame(uint32_t seq, const char *body, char *out, size_t n);
void sendFrame(const char *frame);
void sendCommand(const char *cmd);
void expectAck(uint32_t seq);
bool takeAck(AckInfo &out);
bool takeAckBad();

bool execBlocking(const char *body, AckInfo &out, uint32_t perAttemptMs, uint8_t attempts);
void setBootloaderMode(bool on);
HardwareSerial &port();
} // namespace Stm
