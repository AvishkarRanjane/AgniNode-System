#pragma once
#include <WiFiClientSecure.h>

namespace Net {
void begin();   /* Wi-Fi + NTP                                   */
void service(); /* non-blocking reconnect                        */
bool online();  /* Wi-Fi up and clock synchronised (TLS needs it) */
bool wifiConnected();
bool clockValid();
int wifiStatus();
WiFiClientSecure &tls(); /* TLS client pinned to certs/roots.pem          */
} // namespace Net
