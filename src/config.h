#pragma once

#include <Arduino.h>

struct TrackerConfig {
  String wifiSsid;
  String wifiPass;
  String url;           // full request URL, POSTed to as entered
  uint16_t intervalSec; // send interval
  String apPass;        // empty = open config AP

  bool isComplete() const { return wifiSsid.length() > 0 && url.length() > 0; }
};

namespace config {

constexpr uint16_t kDefaultIntervalSec = 5;
constexpr uint16_t kMinIntervalSec = 1;
constexpr uint16_t kMaxIntervalSec = 3600;

void load(TrackerConfig &cfg);
void save(const TrackerConfig &cfg);
void clear();

} // namespace config
