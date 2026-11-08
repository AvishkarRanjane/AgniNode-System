#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace Firestore {
String devicePath(); /* "devices/<DEVICE_ID>" */
int send(const char *method, const String &path, const String &body, String *out = nullptr);
int runQuery(const String &parentPath, const String &body, String *out = nullptr);

void fString(JsonObject f, const char *k, const char *v);
void fBool(JsonObject f, const char *k, bool v);
void fInt(JsonObject f, const char *k, long v);
void fDouble(JsonObject f, const char *k, float v); /* NaN is skipped */
void fTime(JsonObject f, const char *k, time_t t);
void fArray(JsonObject f, const char *k, const float *v, size_t n); /* arrayValue of doubles */
} // namespace Firestore
