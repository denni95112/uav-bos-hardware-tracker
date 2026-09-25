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
  cfg.apPass = prefs.getString("appass", "");
  prefs.end();

  cfg.intervalSec = constrain(cfg.intervalSec, kMinIntervalSec, kMaxIntervalSec);
}

void save(const TrackerConfig &cfg) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
  prefs.putString("url", cfg.url);
  prefs.putUShort("interval", constrain(cfg.intervalSec, kMinIntervalSec, kMaxIntervalSec));
  prefs.putString("appass", cfg.apPass);
  prefs.end();
}

void clear() {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.clear();
  prefs.end();
}

} // namespace config
