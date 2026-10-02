#pragma once

#include <Arduino.h>

#include "gnss.h"
#include "meshproto.h"

// Meshtastic-compatible LoRa mesh node (EU_868 LongFast, private channel, static key).
// The radio runs on its own task; the main loop only schedules own messages and drains received ones.

struct MeshRx {
  uint32_t from = 0;
  uint32_t id = 0;
  uint8_t type = 0; // meshproto::MsgType
  uint8_t hops = 0; // hops travelled
  float snr = 0;
  meshproto::Position pos;
  uint16_t urlHash = 0;
  char url[meshproto::kMaxUrlLen + 1] = {0};
};

struct MeshStats {
  bool ok = false;           // radio initialised and key valid
  bool active = false;       // radio receiving (mode uses LoRa)
  bool placeholderKey = false;
  uint32_t nodeNum = 0;
  uint32_t rxCount = 0;      // own-channel packets decoded
  uint32_t relayCount = 0;
  uint32_t txCount = 0;      // own messages sent
  uint32_t txBlocked = 0;    // own messages dropped by the airtime limit
  uint32_t lastTxMs = 0;     // own position, 0 = never
  uint32_t lastRxMs = 0;
  int16_t lastRssi = 0;
  float lastSnr = 0;
  float airtimePercent = 0;  // last hour
  uint8_t heardNodes = 0;    // trackers heard in the last 30 min
  uint8_t activeNodes = 0;   // trackers heard in the last 5 min
  uint8_t directNodes = 0;   // of those, heard directly (not relayed)
  uint8_t positionNodes1h = 0; // trackers that sent a position in the last hour
  const char *error = "";
};

struct MeshNodeInfo {
  uint32_t node = 0;
  uint32_t lastAgoSec = 0;
  int32_t lastPosAgoSec = -1; // -1 = no position yet
  uint32_t positions = 0;     // positions received since boot
  uint8_t hops = 0;           // hops of the last packet, 0 = direct
  float snr = 0;
  int16_t rssi = 0;
};

namespace mesh {

void begin(); // initialises the radio once and starts the task (radio stays asleep until setActive)
void setActive(bool on);

// Call every loop. Schedules own position (when sendOwn) and credential messages.
void loop(const GnssFix &fix, const String &url, uint16_t intervalSec, bool sendOwn);

bool receive(MeshRx &out); // non-blocking, tracker messages from other nodes
void requestCredentials(uint32_t node);

MeshStats stats();
// Trackers heard in the last hour, most recent first. Returns the number written to `out`.
size_t nodes(MeshNodeInfo *out, size_t max);

} // namespace mesh
