#include "config.h"

#include <Preferences.h>

namespace config {

static const char *kNamespace = "tracker";

void load(TrackerConfig &cfg) {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  cfg.wifiSsid = prefs.getString("ssid", "");
  cfg.wifiPass = prefs.getString("pass", "");
  cfg.url = prefs.getString("url", "");
  cfg.intervalSec = prefs.getUShort("interval", kDefaultIntervalSec);
  cfg.loraIntervalSec = prefs.getUShort("lorainterval", kDefaultLoraIntervalSec);
  cfg.apPass = prefs.getString("appass", "");
  uint8_t mode = prefs.getUChar("mode", (uint8_t)TrackerMode::WifiOnly);
  prefs.end();

  cfg.intervalSec = constrain(cfg.intervalSec, kMinIntervalSec, kMaxIntervalSec);
  cfg.loraIntervalSec = constrain(cfg.loraIntervalSec, kMinLoraIntervalSec, kMaxLoraIntervalSec);
  cfg.mode = mode <= (uint8_t)TrackerMode::LoraOnly ? (TrackerMode)mode : TrackerMode::WifiOnly;
}

void save(const TrackerConfig &cfg) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
  prefs.putString("url", cfg.url);
  prefs.putUShort("interval", constrain(cfg.intervalSec, kMinIntervalSec, kMaxIntervalSec));
  prefs.putUShort("lorainterval", constrain(cfg.loraIntervalSec, kMinLoraIntervalSec, kMaxLoraIntervalSec));
  prefs.putString("appass", cfg.apPass);
  prefs.putUChar("mode", (uint8_t)cfg.mode);
  prefs.end();
}

void saveMode(TrackerMode mode) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putUChar("mode", (uint8_t)mode);
  prefs.end();
}

void clear() {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.clear();
  prefs.end();
}

TrackerMode nextMode(TrackerMode mode) {
  switch (mode) {
  case TrackerMode::WifiOnly: return TrackerMode::Gateway;
  case TrackerMode::Gateway: return TrackerMode::LoraOnly;
  default: return TrackerMode::WifiOnly;
  }
}

const char *modeName(TrackerMode mode) {
  switch (mode) {
  case TrackerMode::Gateway: return "Gateway";
  case TrackerMode::LoraOnly: return "Nur LoRa";
  default: return "Nur WLAN";
  }
}

const char *modeShort(TrackerMode mode) {
  switch (mode) {
  case TrackerMode::Gateway: return "GW";
  case TrackerMode::LoraOnly: return "LoRa";
  default: return "WLAN";
  }
}

} // namespace config
