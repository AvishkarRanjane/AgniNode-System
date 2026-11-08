#pragma once
#include "ota_common.h"

/* Self-update with signature check and automatic rollback:
 *  1. stream the .bin into the inactive app slot while hashing it
 *  2. verify SHA-256 + ECDSA signature BEFORE the boot partition is switched
 *  3. new firmware must connect to Firestore within OTA_HEALTH_UPTIME_MS, otherwise
 *     (or after OTA_MAX_BOOT_ATTEMPTS unconfirmed boots) the previous slot is restored */
namespace OtaEsp32 {
void bootCheck();                                             /* first call in setup() */
bool run(const OtaMeta &m, const String &docId, String &err); /* restarts on success   */
bool pendingConfirmation(String &docId);
void confirm();
bool takeRollbackReport(String &docId);
} // namespace OtaEsp32
