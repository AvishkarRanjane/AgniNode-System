#include "commands.h"
#include "auth.h"
#include "config.h"
#include "firestore.h"
#include "ota_common.h"
#include "ota_esp32.h"
#include "ota_stm32.h"
#include "secrets.h"
#include "stm_link.h"
#include "telemetry.h"
#include "util.h"

namespace Commands {

struct ActionDef {
    const char *action;
    const char *b1;
    const char *b2;
    bool suppression;
};

static const ActionDef ACTIONS[] = {
    {"PING", "PING", nullptr, false},
    {"STATUS", "STATUS", nullptr, false},
    {"CLEAR_FAULT", "CLRFAULT", nullptr, false},
    {"BUZZER_ON", "BUZZER,ON", nullptr, false}, /* + optional durationSec */
    {"BUZZER_OFF", "BUZZER,OFF", nullptr, false},
    {"SUPPRESS_ARM", "SUPPRESS,ARM", nullptr, true},
    {"SUPPRESS_FIRE", "SUPPRESS,FIRE", nullptr, true},
    {"SUPPRESS_DISARM", "SUPPRESS,DISARM", nullptr, false},
    {"SUPPRESS_IGNITE", "SUPPRESS,ARM", "SUPPRESS,FIRE",
     true},                                         /* ARM then FIRE, each ACK-verified */
    {"MAINS_OPEN", "MAINS,OPEN", nullptr, false},   /* SSR CH2 open (manual disconnect)  */
    {"MAINS_CLOSE", "MAINS,CLOSE", nullptr, false}, /* refused while a trip is latched   */
    {"TRIP_RESET", "TRIP,RESET", nullptr, false},   /* clear latched RCD/AFDD trips      */
};

struct InFlight {
    bool active = false;
    String docId;
    char bodies[2][24];
    uint8_t n = 0, idx = 0, attempts = 0;
    uint32_t seq = 0, lastSendMs = 0;
    char frame[112];
};

struct FinalResult {
    bool pending = false;
    String docId;
    char status[12];
    bool hasAck = false;
    AckInfo ack;
    uint32_t seq = 0;
    char err[40];
    char version[16];
    uint8_t tries = 0;
    uint32_t lastTryMs = 0;
};

static InFlight cur;
static FinalResult fin;
static uint32_t lastPollMs = 0;
static bool authWaitReported = false;

void begin() {
    OtaStm::begin();
}

/* ------------------------------------------------------------ document updates */
static bool updateCommand(const String &docId, const char *status, const AckInfo *ack, uint32_t seq,
                          const char *err, const char *version) {
    JsonDocument doc;
    JsonObject f = doc["fields"].to<JsonObject>();
    time_t now = time(nullptr);
    bool terminal = strcmp(status, "sent") != 0 && strcmp(status, "running") != 0;

    Firestore::fString(f, "status", status);
    Firestore::fTime(f, "updatedAt", now);
    if (terminal)
        Firestore::fTime(f, "completedAt", now);
    if (seq)
        f["seq"]["integerValue"] = String((unsigned long)seq);
    if (err && err[0])
        Firestore::fString(f, "error", err);
    if (version && version[0])
        Firestore::fString(f, "firmwareVersion", version);
    if (ack) {
        Firestore::fString(f, "ackResult", ack->result);
        Firestore::fString(f, "ackDetail", ack->detail);
        Firestore::fBool(f, "buzzerOn", ack->buz != 0);
        Firestore::fBool(f, "ssrOn", ack->ssr != 0);
        Firestore::fBool(f, "armed", ack->arm != 0);
        Firestore::fString(f, "stm32State", ack->state);
        Firestore::fInt(f, "stm32Faults", ack->faults);
        Firestore::fBool(f, "mainsClosed", ack->mains != 0);
        Firestore::fInt(f, "tripMask", ack->trip);
    }

    String path = Firestore::devicePath() + "/commands/" + docId + "?";
    bool first = true;
    for (JsonPair kv : f) {
        if (!first)
            path += "&";
        path += "updateMask.fieldPaths=";
        path += kv.key().c_str();
        first = false;
    }
    String body;
    serializeJson(doc, body);
    int c = Firestore::send("PATCH", path, body);
    return c >= 200 && c < 300;
}

static void queueFinal(const String &docId, const char *status, const AckInfo *ack, uint32_t seq,
                       const char *err, const char *version) {
    fin.pending = true;
    fin.docId = docId;
    strlcpy(fin.status, status, sizeof(fin.status));
    fin.hasAck = (ack != nullptr);
    if (ack)
        fin.ack = *ack;
    fin.seq = seq;
    strlcpy(fin.err, err ? err : "", sizeof(fin.err));
    strlcpy(fin.version, version ? version : "", sizeof(fin.version));
    fin.tries = 0;
    fin.lastTryMs = 0;
}

/* ------------------------------------------------------------ serial commands */
static void sendCurrent(bool newSeq) {
    if (newSeq) {
        cur.seq = Stm::nextSeq();
        Stm::buildFrame(cur.seq, cur.bodies[cur.idx], cur.frame, sizeof(cur.frame));
        cur.attempts = 1;
    }
    cur.lastSendMs = millis();
    Stm::expectAck(cur.seq);
    Serial.println("[UART TX] Forwarding command to STM32 over direct UART...");
    Stm::sendFrame(cur.frame);
    Serial.printf("[Cmd] %s -> STM32: %s", cur.docId.c_str(), cur.frame);
}

static void finishCurrent(const char *status, const AckInfo *ack, const char *err) {
    queueFinal(cur.docId, status, ack, cur.seq, err, nullptr);
    Serial.printf("[Cmd] %s finished: %s\n", cur.docId.c_str(), status);
    cur.active = false;
    Stm::markLiveDirty();
}

static float numberField(JsonObjectConst v) {
    if (v["doubleValue"].is<float>())
        return v["doubleValue"].as<float>();
    if (v["integerValue"].is<const char *>())
        return (float)atof(v["integerValue"].as<const char *>());
    return NAN;
}

static void startOta(const String &docId, const String &action, JsonObjectConst fields) {
#if !ENABLE_OTA
    updateCommand(docId, "rejected", nullptr, 0, "OTA_DISABLED", nullptr);
    return;
#else
    OtaMeta m;
    String err;
    if (!OtaCommon::parseMeta(fields, m, err)) {
        updateCommand(docId, "rejected", nullptr, 0, err.c_str(), nullptr);
        return;
    }
    if (!updateCommand(docId, "running", nullptr, 0, nullptr, nullptr))
        return; /* claim */

    bool ok;
    if (action == "OTA_ESP32")
        ok = OtaEsp32::run(m, docId, err); /* restarts on success */
    else
        ok = OtaStm::run(m, err);

    if (ok)
        queueFinal(docId, "completed", nullptr, 0, nullptr, Stm::tel().fw);
    else
        queueFinal(docId, "failed", nullptr, 0, err.c_str(), nullptr);
#endif
}

static void startCommand(const String &docId, const String &action, float duration, time_t created,
                         JsonObjectConst fields) {
    time_t now = time(nullptr);
    if (created > 0 && (now - created) > CMD_MAX_AGE_S) {
        updateCommand(docId, "expired", nullptr, 0, "COMMAND_TOO_OLD", nullptr);
        return;
    }

    if (action == "OTA_ESP32" || action == "OTA_STM32") {
        startOta(docId, action, fields);
        return;
    }

    const ActionDef *def = nullptr;
    for (const ActionDef &a : ACTIONS)
        if (action == a.action) {
            def = &a;
            break;
        }
    if (!def) {
        updateCommand(docId, "rejected", nullptr, 0, "UNKNOWN_ACTION", nullptr);
        return;
    }
#if !ALLOW_REMOTE_IGNITION
    if (def->suppression) {
        updateCommand(docId, "rejected", nullptr, 0, "SUPPRESSION_DISABLED_ON_GATEWAY", nullptr);
        return;
    }
#endif

    if (!Stm::online()) {
        updateCommand(docId, "rejected", nullptr, 0, "STM32_OFFLINE", nullptr);
        return;
    }

    memset(cur.bodies, 0, sizeof(cur.bodies));
    strlcpy(cur.bodies[0], def->b1, sizeof(cur.bodies[0]));
    if (def->b2)
        strlcpy(cur.bodies[1], def->b2, sizeof(cur.bodies[1]));
    cur.n = def->b2 ? 2 : 1;
    if (action == "BUZZER_ON" && !isnan(duration)) {
        long d = (long)duration;
        if (d < 1 || d > 600) {
            updateCommand(docId, "rejected", nullptr, 0, "BAD_DURATION", nullptr);
            return;
        }
        snprintf(cur.bodies[0], sizeof(cur.bodies[0]), "BUZZER,ON,%ld", d);
    }

    /* claim BEFORE transmitting: a command is executed at most once */
    if (!updateCommand(docId, "sent", nullptr, 0, nullptr, nullptr))
        return;

    cur.active = true;
    cur.docId = docId;
    cur.idx = 0;
    sendCurrent(true);
}

static void pollCommands() {
    JsonDocument q;
    JsonObject sq = q["structuredQuery"].to<JsonObject>();
    sq["from"][0]["collectionId"] = "commands";
    JsonObject ff = sq["where"]["fieldFilter"].to<JsonObject>();
    ff["field"]["fieldPath"] = "status";
    ff["op"] = "EQUAL";
    ff["value"]["stringValue"] = "pending";
    sq["limit"] = 5;

    String body, resp;
    serializeJson(q, body);
    String parentPath = Firestore::devicePath();
    Serial.printf("[Cmd] polling pending commands; parent=%s collection=commands\n",
                  parentPath.c_str());
    int code = Firestore::runQuery(parentPath, body, &resp);
    if (code != 200) {
        Serial.printf("[Cmd] Firestore poll failed (HTTP %d)\n", code);
        return;
    }

    JsonDocument res;
    DeserializationError error = deserializeJson(res, resp);
    if (error) {
        Serial.printf("[Cmd] Firestore poll response parse failed: %s\n", error.c_str());
        return;
    }

    JsonObject best;
    time_t bestCreated = 0;
    String bestId;
    uint8_t pendingCount = 0;
    for (JsonObject item : res.as<JsonArray>()) {
        JsonObject d = item["document"];
        if (d.isNull())
            continue;
        pendingCount++;
        String name = d["name"].as<String>();
        String id = name.substring(name.lastIndexOf('/') + 1);
        if (!Util::validDocId(id))
            continue;

        time_t created = 0;
        if (d["fields"]["createdAt"]["timestampValue"].is<const char *>())
            created = Util::isoToEpoch(d["fields"]["createdAt"]["timestampValue"]);
        if (created == 0)
            created = Util::isoToEpoch(d["createTime"] | "");

        if (best.isNull() || (created != 0 && created < bestCreated)) {
            best = d;
            bestCreated = created;
            bestId = id;
        }
    }
    if (best.isNull()) {
        Serial.printf("[Cmd] Firestore poll OK (pending documents=%u; no valid command selected)\n",
                      (unsigned)pendingCount);
        return;
    }

    JsonObject fields = best["fields"];
    String action = fields["action"]["stringValue"] | "";
    Serial.printf("[Cmd] Firestore poll OK (pending documents=%u); selected id=%s action=%s\n",
                  (unsigned)pendingCount, bestId.c_str(), action.c_str());
    Serial.printf("[FIREBASE RX] Command Received from Website: %s\n", action.c_str());
    startCommand(bestId, action, numberField(fields["durationSec"]), bestCreated, fields);
}

/* ------------------------------------------------------------ main service */
static void reportOtaOutcomes() {
    static bool confirmed = false;
    String doc;
    if (OtaEsp32::takeRollbackReport(doc)) {
        queueFinal(doc, "failed", nullptr, 0, "ROLLED_BACK_NEW_FIRMWARE_UNHEALTHY", nullptr);
        return;
    }
    if (!confirmed && OtaEsp32::pendingConfirmation(doc) && millis() > OTA_HEALTH_UPTIME_MS &&
        Telemetry_::lastUploadOk()) {
        OtaEsp32::confirm();
        confirmed = true;
        queueFinal(doc, "completed", nullptr, 0, nullptr, FW_VERSION_ESP);
    }
}

void service() {
    if (!Auth::ensure()) {
        if (!authWaitReported)
            Serial.println("[Cmd] waiting for Firebase Auth");
        authWaitReported = true;
        return;
    }
    authWaitReported = false;
    uint32_t now = millis();

    if (!fin.pending && !cur.active)
        reportOtaOutcomes();

    /* 1. push the result of the previous command (retry on failure) */
    if (fin.pending && (fin.lastTryMs == 0 || now - fin.lastTryMs >= 2000UL)) {
        fin.lastTryMs = now ? now : 1;
        bool ok = updateCommand(fin.docId, fin.status, fin.hasAck ? &fin.ack : nullptr, fin.seq,
                                fin.err, fin.version);
        if (ok || ++fin.tries >= FINAL_UPDATE_RETRIES)
            fin.pending = false;
        return;
    }

    /* 2. command in flight: wait for ACK, retry with the SAME seq, or time out */
    if (cur.active) {
        AckInfo a;
        if (Stm::takeAck(a)) {
            bool ok = !strcmp(a.result, "OK");
            if (strncmp(cur.bodies[cur.idx], "BUZZER,", 7) == 0) {
                Serial.printf("[STM32 ACK] Alarm command on PA6 -> Status: %s, result=%s\n",
                              a.buz ? "ALARM IS ON" : "ALARM IS OFF", a.result);
            }
            if (!ok)
                finishCurrent("rejected", &a, a.detail);
            else if (cur.idx + 1 < cur.n) {
                cur.idx++;
                sendCurrent(true);
            } /* IGNITE: ARM done -> FIRE */
            else
                finishCurrent("completed", &a, nullptr);
        } else if (Stm::takeAckBad() || (now - cur.lastSendMs) >= ACK_TIMEOUT_MS) {
            if (cur.attempts < CMD_MAX_ATTEMPTS) {
                cur.attempts++;
                sendCurrent(false);
            } else
                finishCurrent("timeout", nullptr, "NO_ACK_FROM_STM32");
        }
        return;
    }

    /* 3. idle: poll Firestore for new commands */
    if (!fin.pending && (now - lastPollMs) >= COMMAND_POLL_MS) {
        lastPollMs = now;
        pollCommands();
    }
}

} // namespace Commands
