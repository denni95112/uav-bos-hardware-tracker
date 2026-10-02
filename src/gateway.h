#pragma once

#include <Arduino.h>

#include "gnss.h"
#include "uplink.h"

// Forwards positions received over the LoRa mesh to the UAV-BOS URL of the sending tracker.
// The URL comes from the tracker's own credentials message and is cached per node.

struct GatewayStats {
  uint32_t forwarded = 0;
  uint32_t failed = 0;
  uint32_t dropped = 0; // stale, unknown URL or no internet
  uint8_t knownNodes = 0;
  UplinkStatus up;      // last forward attempt
};

namespace gateway {

void begin(); // loads the URL cache from NVS

// Drains received mesh messages. Positions are POSTed only when `forward` is set (Gateway mode + WiFi up);
// credentials are always cached. At most one HTTP request per call.
void loop(bool forward, const GnssFix &ownFix);

const GatewayStats &stats();

} // namespace gateway
