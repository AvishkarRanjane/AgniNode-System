#pragma once

/* Firestore -> STM32 command bridge.
 *   devices/<id>/commands/<doc>:  action, status="pending", createdAt, [durationSec], [url, sha256,
 * signature, version, force] status flow: pending -> sent|running -> completed | rejected | failed
 * | timeout | expired */
namespace Commands {
void begin();
void service();
} // namespace Commands
