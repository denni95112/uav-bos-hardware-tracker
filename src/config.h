#pragma once

#include <Arduino.h>

enum class TrackerMode : uint8_t {
  WifiOnly = 0, // WiFi uplink, LoRa radio asleep
  Gateway = 1,  // WiFi uplink + LoRa mesh node that forwards client positions
  LoraOnly = 2, // WiFi off, own position goes through the LoRa mesh
};

// Meshtastic modem presets that fit the EU_868 sub-band 869.4-869.65 MHz.
enum class LoraPreset : uint8_t {
  ShortFast = 0,
  ShortSlow = 1,
  MediumFast = 2,
  MediumSlow = 3,
  LongFast = 4,
  LongModerate = 5,
  LongSlow = 6,
  Count
};

enum class RelayMode : uint8_t {
  All = 0,     // like Meshtastic rebroadcast mode ALL
  OwnOnly = 1, // only packets on the own tracker channel
  None = 2,
};

struct MeshSettings {
  LoraPreset preset = LoraPreset::LongFast;
  uint8_t slot = 0; // frequency slot, 0 = Meshtastic default for the preset
  uint8_t hopLimit = 3;
  int8_t txPowerDbm = 22;
  RelayMode relayMode = RelayMode::All;
  uint8_t foreignAirtimePct = 6; // foreign packets are relayed only below this share of airtime
};

struct TrackerConfig {
  String wifiSsid;
  String wifiPass;
  String url;               // full request URL, POSTed to as entered
  uint16_t intervalSec;     // send interval over WiFi
  uint16_t loraIntervalSec; // send interval over LoRa
  String apPass;            // empty = open config AP
  TrackerMode mode = TrackerMode::WifiOnly;
  MeshSettings mesh;
  String meshKey;           // base64 channel key, same on all trackers of an organisation

  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasUrl() const { return url.length() > 0; }
  bool usesWifi() const { return mode != TrackerMode::LoraOnly; }
  bool usesLora() const { return mode != TrackerMode::WifiOnly; }
};

namespace config {

constexpr uint16_t kDefaultIntervalSec = 5;
constexpr uint16_t kMinIntervalSec = 1;
constexpr uint16_t kMaxIntervalSec = 3600;

constexpr uint16_t kDefaultLoraIntervalSec = 30;
constexpr uint16_t kMinLoraIntervalSec = 15;
constexpr uint16_t kMaxLoraIntervalSec = 3600;

constexpr uint8_t kMaxSlot = 2;
constexpr uint8_t kMinHopLimit = 1;
constexpr uint8_t kMaxHopLimit = 7; // 3 bits in the Meshtastic header
constexpr int8_t kMinTxPowerDbm = 2;
constexpr int8_t kMaxTxPowerDbm = 22; // SX1262 maximum
constexpr uint8_t kMaxForeignAirtimePct = 10;

// Used when neither NVS nor the build provides a mesh key; the UI warns about it.
constexpr const char *kPlaceholderMeshKey = "CHANGE+ME+CHANGE+ME+CHANGE+ME+CHANGE+ME+CEE=";

// Clamps every field to its valid range.
MeshSettings sanitize(MeshSettings s);
// Base64 that decodes to a 16 or 32 byte AES key.
bool validMeshKey(const String &b64);

void load(TrackerConfig &cfg);
void save(const TrackerConfig &cfg);
void saveMode(TrackerMode mode);
void clear();

TrackerMode nextMode(TrackerMode mode);
const char *modeName(TrackerMode mode);  // long German name for the UI
const char *modeShort(TrackerMode mode); // badge text

} // namespace config
