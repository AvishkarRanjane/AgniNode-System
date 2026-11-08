#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <mbedtls/sha256.h>

/* Fields of an OTA_* command document (Firestore "fields" object) */
struct OtaMeta {
    String url;     /* https://firebasestorage.googleapis.com/...  (prefix enforced) */
    String sha256;  /* 64 hex chars of the .bin                                       */
    String sig;     /* base64 DER ECDSA-P256 signature over the SHA-256 digest        */
    String version; /* optional, checked against the running version after flashing   */
    bool force = false;
};

namespace OtaCommon {

class Sha256 {
  public:
    Sha256() {
        mbedtls_sha256_init(&ctx_);
    }
    ~Sha256() {
        mbedtls_sha256_free(&ctx_);
    }
    void begin() {
        mbedtls_sha256_starts_ret(&ctx_, 0);
    }
    void update(const uint8_t *d, size_t n) {
        mbedtls_sha256_update_ret(&ctx_, d, n);
    }
    void finish(uint8_t out[32]) {
        mbedtls_sha256_finish_ret(&ctx_, out);
    }

  private:
    mbedtls_sha256_context ctx_;
};

bool parseMeta(JsonObjectConst fields, OtaMeta &m, String &err);
bool verify(const uint8_t hash[32], const OtaMeta &m,
            String &err); /* digest match + ECDSA signature */
bool openStream(HTTPClient &http, const String &url, int &len, String &err);

} // namespace OtaCommon
