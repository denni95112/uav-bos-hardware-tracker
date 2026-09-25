#pragma once

#include <Arduino.h>

#include "gnss.h"

struct UplinkStatus {
  uint32_t lastAttemptMs = 0; // millis() of the last send attempt, 0 = never
  uint32_t lastSuccessMs = 0;
  int lastHttpCode = 0;       // >0 HTTP status, <0 HTTPClient error
  uint32_t sentCount = 0;
  uint32_t failCount = 0;
  uint16_t consecutiveFails = 0;
  String lastError;
  String lastPayload;
};

namespace uplink {

void begin(const String &url);
bool send(const GnssFix &fix);
String buildJson(const GnssFix &fix);
const UplinkStatus &status();

// Delay until the next send: the configured interval, doubled per consecutive failure, max 60 s.
uint32_t nextDelayMs(uint16_t intervalSec);

} // namespace uplink
