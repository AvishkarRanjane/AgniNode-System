#pragma once
/* Copy to include/secrets.h (git-ignored) and fill in. */

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define FIREBASE_API_KEY "YOUR_FIREBASE_WEB_API_KEY"
#define FIREBASE_PROJECT_ID "YOUR_PROJECT_ID"
#define DEVICE_AUTH_EMAIL "device001@yourdomain.com"
#define DEVICE_AUTH_PASSWORD "DEVICE_ACCOUNT_PASSWORD"
#define DEVICE_ID "agninode-001"

/* ECDSA P-256 public key that verifies firmware signatures (OTA).
 * openssl ecparam -name prime256v1 -genkey -noout -out ota_private.pem
 * openssl ec -in ota_private.pem -pubout -out ota_public.pem            */
#define OTA_PUBLIC_KEY_PEM                                                                         \
    "-----BEGIN PUBLIC KEY-----\n"                                                                 \
    "PASTE_YOUR_BASE64_KEY_LINES_HERE\n"                                                           \
    "-----END PUBLIC KEY-----\n"
