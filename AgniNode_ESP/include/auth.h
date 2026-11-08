#pragma once
#include <Arduino.h>

/* Firebase Auth (email/password) with refresh-token renewal.
 * The refresh token is kept in NVS; tokens are never printed. */
namespace Auth {
void begin();
bool ensure(); /* true when a valid ID token is available */
const String &token();
void invalidate(); /* call after HTTP 401/403 */
} // namespace Auth
