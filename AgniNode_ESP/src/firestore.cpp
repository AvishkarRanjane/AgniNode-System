#include "firestore.h"
#include "auth.h"
#include "config.h"
#include "net.h"
#include "secrets.h"
#include "util.h"
#include <HTTPClient.h>
#include <math.h>

namespace Firestore {

String devicePath() {
    return String("devices/") + DEVICE_ID;
}

static void logApiError(const String &resp) {
    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        Serial.println(
            "[Firestore] structured API error details unavailable (empty or non-JSON response)");
        return;
    }
    const char *status = doc["error"]["status"];
    const char *message = doc["error"]["message"];
    if (status || message) {
        char summary[128];
        strlcpy(summary, message ? message : "no message", sizeof(summary));
        Serial.printf("[Firestore] API error status=%s message=%s\n", status ? status : "unknown",
                      summary);
    }
}

/* method: "PATCH" (create/overwrite a named doc), "POST" (auto-ID doc or :runQuery) */
int send(const char *method, const String &path, const String &body, String *out) {
    if (!Auth::ensure()) {
        Serial.printf("[Firestore] %s %s skipped: Firebase Auth unavailable\n", method,
                      path.c_str());
        return -1;
    }

    HTTPClient http;
    String url = String("https://firestore.googleapis.com/v1/projects/") + FIREBASE_PROJECT_ID +
                 "/databases/(default)/documents/" + path;

    http.setTimeout(HTTP_TIMEOUT_MS);
    uint32_t startedMs = millis();
    if (!http.begin(Net::tls(), url)) {
        Serial.printf("[Firestore] %s %s HTTP begin failed after %lu ms\n", method, path.c_str(),
                      (unsigned long)(millis() - startedMs));
        return -1;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + Auth::token());

    int code = http.sendRequest(method, body);
    String resp = http.getString();
    Serial.printf("[Firestore] %s %s -> HTTP %d, duration=%lu ms, response_bytes=%u%s\n", method,
                  path.c_str(), code, (unsigned long)(millis() - startedMs),
                  (unsigned)resp.length(), code < 0 ? http.errorToString(code).c_str() : "");
    if (code < 200 || code >= 300)
        logApiError(resp);
    if (code == 401 || code == 403)
        Auth::invalidate();
    if (out)
        *out = resp;
    http.end();
    return code;
}

int runQuery(const String &parentPath, const String &body, String *out) {
    String endpoint = parentPath + ":runQuery";
    Serial.printf("[Firestore] runQuery endpoint=/documents/%s\n", endpoint.c_str());
    return send("POST", endpoint, body, out);
}

void fString(JsonObject f, const char *k, const char *v) {
    f[k]["stringValue"] = v;
}
void fBool(JsonObject f, const char *k, bool v) {
    f[k]["booleanValue"] = v;
}
void fInt(JsonObject f, const char *k, long v) {
    f[k]["integerValue"] = String(v);
}
void fDouble(JsonObject f, const char *k, float v) {
    if (!isnan(v))
        f[k]["doubleValue"] = v;
}
void fTime(JsonObject f, const char *k, time_t t) {
    f[k]["timestampValue"] = Util::isoTime(t);
}
void fArray(JsonObject f, const char *k, const float *v, size_t n) {
    JsonArray a = f[k]["arrayValue"]["values"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) {
        JsonObject o = a.add<JsonObject>();
        o["doubleValue"] = isnan(v[i]) ? 0.0f : v[i];
    }
}

} // namespace Firestore
