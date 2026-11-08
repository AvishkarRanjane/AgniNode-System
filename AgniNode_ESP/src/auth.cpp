#include "auth.h"
#include "config.h"
#include "net.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>

namespace Auth {

static String idToken;
static String refreshToken;
static uint32_t expiryMs = 0;
static bool have = false;
static uint32_t lastFailMs = 0;

static void logAuthError(const char *operation, int code, const String &resp) {
    JsonDocument doc;
    const char *message = nullptr;
    if (!deserializeJson(doc, resp))
        message = doc["error"]["message"];
    if (message && message[0]) {
        char summary[96];
        strlcpy(summary, message, sizeof(summary));
        Serial.printf("[Auth] %s failed (HTTP %d, firebase=%s); response body withheld\n",
                      operation, code, summary);
    } else {
        Serial.printf("[Auth] %s failed (HTTP %d); response body withheld\n", operation, code);
    }
}

static int httpPost(const String &url, const char *ctype, const String &body, String &resp) {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    uint32_t startedMs = millis();
    if (!http.begin(Net::tls(), url)) {
        Serial.printf("[Auth] HTTP begin failed after %lu ms\n",
                      (unsigned long)(millis() - startedMs));
        return -1;
    }
    http.addHeader("Content-Type", ctype);
    int code = http.POST(body);
    resp = http.getString();
    Serial.printf("[Auth] HTTP POST completed: status=%d, duration=%lu ms, response_bytes=%u%s\n",
                  code, (unsigned long)(millis() - startedMs), (unsigned)resp.length(),
                  code < 0 ? http.errorToString(code).c_str() : "");
    http.end();
    return code;
}

static bool applyResponse(const String &resp, const char *idKey, const char *rtKey,
                          const char *expKey) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, resp);
    if (error) {
        Serial.printf("[Auth] response JSON parse failed: %s\n", error.c_str());
        return false;
    }
    const char *id = doc[idKey];
    const char *rt = doc[rtKey];
    if (!id || !rt) {
        Serial.printf("[Auth] successful HTTP response is missing expected token fields (%s/%s)\n",
                      idKey, rtKey);
        return false;
    }

    idToken = id;
    if (refreshToken != rt) {
        refreshToken = rt;
        Preferences p;
        p.begin("auth", false);
        p.putString("rt", refreshToken);
        p.end();
    }
    const char *e = doc[expKey] | "3600";
    uint32_t expiresIn = (uint32_t)atoi(e);
    if (expiresIn < 600)
        expiresIn = 600;
    expiryMs = millis() + (expiresIn - 300UL) * 1000UL; /* renew 5 min early */
    have = true;
    return true;
}

static bool signIn() {
    JsonDocument req;
    req["email"] = DEVICE_AUTH_EMAIL;
    req["password"] = DEVICE_AUTH_PASSWORD;
    req["returnSecureToken"] = true;
    String body, resp;
    serializeJson(req, body);

    int code = httpPost(
        String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") +
            FIREBASE_API_KEY,
        "application/json", body, resp);
    if (code != 200 || !applyResponse(resp, "idToken", "refreshToken", "expiresIn")) {
        logAuthError("sign-in", code, resp);
        return false;
    }
    Serial.println("[Auth] sign-in succeeded; ID token acquired");
    return true;
}

static bool refresh() {
    String body = "grant_type=refresh_token&refresh_token=" + refreshToken;
    String resp;
    int code =
        httpPost(String("https://securetoken.googleapis.com/v1/token?key=") + FIREBASE_API_KEY,
                 "application/x-www-form-urlencoded", body, resp);
    if (code != 200 || !applyResponse(resp, "id_token", "refresh_token", "expires_in")) {
        logAuthError("refresh", code, resp);
        return false;
    }
    Serial.println("[Auth] refresh succeeded; ID token renewed");
    return true;
}

void begin() {
    Preferences p;
    p.begin("auth", true);
    refreshToken = p.getString("rt", "");
    p.end();
    Serial.printf("[Auth] initialized; stored refresh token %s; network authentication deferred "
                  "until cloud is ready\n",
                  refreshToken.length() ? "available" : "not found");
}

bool ensure() {
    if (have && (int32_t)(expiryMs - millis()) > 0)
        return true;
    if (lastFailMs && millis() - lastFailMs < 15000UL)
        return false; /* back off */

    Serial.printf("[Auth] acquiring ID token using %s\n",
                  refreshToken.length() ? "stored refresh token" : "email/password sign-in");
    bool ok = (refreshToken.length() && refresh()) || signIn();
    if (!ok) {
        lastFailMs = millis();
        have = false;
        Serial.println("[Auth] authentication unavailable; next attempt no sooner than 15 seconds");
    } else {
        lastFailMs = 0;
    }
    return ok;
}

const String &token() {
    return idToken;
}

void invalidate() {
    have = false;
}

} // namespace Auth
