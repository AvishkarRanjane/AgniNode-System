#include "ota_common.h"
#include "config.h"
#include "net.h"
#include "secrets.h"
#include "util.h"
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>

namespace OtaCommon {

bool parseMeta(JsonObjectConst f, OtaMeta &m, String &err) {
    m.url = f["url"]["stringValue"] | "";
    m.sha256 = f["sha256"]["stringValue"] | "";
    m.sig = f["signature"]["stringValue"] | "";
    m.version = f["version"]["stringValue"] | "";
    m.force = f["force"]["booleanValue"] | false;

    if (!m.url.startsWith("https://") || !m.url.startsWith(OTA_URL_PREFIX)) {
        err = "URL_NOT_ALLOWED";
        return false;
    }
    uint8_t tmp[32];
    if (!Util::hexToBytes(m.sha256.c_str(), tmp, 32)) {
        err = "BAD_SHA256";
        return false;
    }
    if (m.sig.length() < 80) {
        err = "MISSING_SIGNATURE";
        return false;
    }
    return true;
}

bool verify(const uint8_t hash[32], const OtaMeta &m, String &err) {
    uint8_t expected[32];
    if (!Util::hexToBytes(m.sha256.c_str(), expected, 32) || memcmp(expected, hash, 32) != 0) {
        err = "SHA256_MISMATCH";
        return false;
    }

    uint8_t sig[128];
    size_t sigLen = 0;
    if (mbedtls_base64_decode(sig, sizeof(sig), &sigLen, (const uint8_t *)m.sig.c_str(),
                              m.sig.length()) != 0) {
        err = "BAD_SIGNATURE_ENCODING";
        return false;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    const char *key = OTA_PUBLIC_KEY_PEM;
    bool ok = false;
    if (mbedtls_pk_parse_public_key(&pk, (const uint8_t *)key, strlen(key) + 1) != 0) {
        err = "BAD_PUBLIC_KEY";
    } else if (mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, sigLen) != 0) {
        err = "SIGNATURE_INVALID";
    } else {
        ok = true;
    }
    mbedtls_pk_free(&pk);
    return ok;
}

bool openStream(HTTPClient &http, const String &url, int &len, String &err) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(Net::tls(), url)) {
        err = "HTTP_BEGIN_FAILED";
        return false;
    }
    int code = http.GET();
    if (code != 200) {
        err = "HTTP_" + String(code);
        http.end();
        return false;
    }
    len = http.getSize();
    if (len <= 0) {
        err = "NO_CONTENT_LENGTH";
        http.end();
        return false;
    }
    return true;
}

} // namespace OtaCommon
