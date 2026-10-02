#pragma once

#include <Arduino.h>

#include "config.h"
#include "gnss.h"
#include "meshproto.h"

// Meshtastic-compatible LoRa mesh node (EU_868, configurable modem preset, private channel, shared key).
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
  uint32_t relayCount = 0;   // all relayed packets
  uint32_t ownRelayCount = 0;     // relayed packets on the own tracker channel
  uint32_t foreignRelayCount = 0; // relayed packets of other Meshtastic channels
  uint32_t foreignDropped = 0;    // foreign packets not relayed because of the airtime budget
  uint32_t viaForeignCount = 0;   // own-channel packets whose last relay was not a known tracker
  uint32_t credRetries = 0;       // request URL repeated because no relay was heard
  uint32_t positionsNotRelayed = 0; // positions delivered by this gateway instead of relayed
  uint8_t intervalScale = 1;      // own LoRa interval stretched because of high airtime
  uint32_t txCount = 0;      // own messages sent
  uint32_t txBlocked = 0;    // own messages dropped by the airtime limit
  uint32_t lastTxMs = 0;     // own position, 0 = never
  uint32_t lastRxMs = 0;
  int16_t lastRssi = 0;
  float lastSnr = 0;
  float airtimePercent = 0;  // last hour
  float trackerAirtimePercent = 0; // of that, own messages and own-channel relays
  uint8_t heardNodes = 0;    // trackers heard in the last 30 min
  uint8_t activeNodes = 0;   // trackers heard in the last 5 min
  uint8_t directNodes = 0;   // of those, heard directly (not relayed)
  uint8_t positionNodes1h = 0; // trackers that sent a position in the last hour
  const char *error = "";
  // active radio configuration
  const char *presetName = "";
  float freqMhz = 0;
  float bandwidthKhz = 0;
  uint8_t spreadingFactor = 0;
  uint8_t codingRate = 0; // 4/x
  uint8_t hopLimit = 0;
  int8_t txPowerDbm = 0;
};

struct MeshNodeInfo {
  uint32_t node = 0;
  uint32_t lastAgoSec = 0;
  int32_t lastPosAgoSec = -1; // -1 = no position yet
  uint32_t positions = 0;     // positions received since boot
  uint8_t hops = 0;           // hops of the last packet, 0 = direct
  bool viaForeign = false;    // last packet was relayed by a non-tracker node (e.g. Meshtastic)
  float snr = 0;
  int16_t rssi = 0;
};

namespace mesh {

// Initialises the radio once and starts the task (radio stays asleep until setActive).
// Settings take effect on the first call only; changes need a restart.
void begin(const MeshSettings &settings, const String &keyB64);
void setActive(bool on);
// Gateway whose uplink recently succeeded: positions of deliverable trackers are forwarded, not relayed.
void setUplinkOnline(bool online);
// Trackers whose request URL (matching urlHash) the gateway has cached.
void setDeliverable(uint32_t node, uint16_t urlHash);
void clearDeliverable(uint32_t node);

// Call every loop. Schedules own position (when sendOwn) and credential messages.
void loop(const GnssFix &fix, const String &url, uint16_t intervalSec, bool sendOwn);

bool receive(MeshRx &out); // non-blocking, tracker messages from other nodes
void requestCredentials(uint32_t node);

MeshStats stats();
// Trackers heard in the last hour, most recent first. Returns the number written to `out`.
size_t nodes(MeshNodeInfo *out, size_t max);

} // namespace mesh
