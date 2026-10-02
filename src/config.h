#pragma once

#include <Arduino.h>

enum class TrackerMode : uint8_t {
  WifiOnly = 0, // WiFi uplink, LoRa radio asleep
  Gateway = 1,  // WiFi uplink + LoRa mesh node that forwards client positions
  LoraOnly = 2, // WiFi off, own position goes through the LoRa mesh
};

struct TrackerConfig {
  String wifiSsid;
  String wifiPass;
  String url;               // full request URL, POSTed to as entered
  uint16_t intervalSec;     // send interval over WiFi
  uint16_t loraIntervalSec; // send interval over LoRa
  String apPass;            // empty = open config AP
  TrackerMode mode = TrackerMode::WifiOnly;

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

void load(TrackerConfig &cfg);
void save(const TrackerConfig &cfg);
void saveMode(TrackerMode mode);
void clear();

TrackerMode nextMode(TrackerMode mode);
const char *modeName(TrackerMode mode);  // long German name for the UI
const char *modeShort(TrackerMode mode); // badge text

} // namespace config
