#include "config.h"

#include <Preferences.h>
#include <mbedtls/base64.h>

#ifndef MESH_PSK_B64
#define MESH_PSK_B64 ""
#endif

namespace config {

static const char *kNamespace = "tracker";

// A key stored on the device wins over the build key, so firmware updates never swap keys.
// Devices without a stored key adopt the build key once, which migrates older installations.
static String loadMeshKey() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  String key = prefs.getString("meshkey", "");
  prefs.end();
  if (validMeshKey(key)) return key;
  key = MESH_PSK_B64;
  if (!validMeshKey(key) || key == kPlaceholderMeshKey) return kPlaceholderMeshKey;
  prefs.begin(kNamespace, false);
  prefs.putString("meshkey", key);
  prefs.end();
  return key;
}

bool validMeshKey(const String &b64) {
  if (b64.isEmpty() || b64.length() > 64) return false;
  uint8_t buf[48];
  size_t olen = 0;
  if (mbedtls_base64_decode(buf, sizeof(buf), &olen, (const unsigned char *)b64.c_str(), b64.length()) != 0)
    return false;
  return olen == 16 || olen == 32;
}

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
  MeshSettings m;
  m.preset = (LoraPreset)prefs.getUChar("preset", (uint8_t)m.preset);
  m.slot = prefs.getUChar("slot", m.slot);
  m.hopLimit = prefs.getUChar("hops", m.hopLimit);
  m.txPowerDbm = prefs.getChar("txpower", m.txPowerDbm);
  m.relayMode = (RelayMode)prefs.getUChar("relay", (uint8_t)m.relayMode);
  m.foreignAirtimePct = prefs.getUChar("fairtime", m.foreignAirtimePct);
  prefs.end();
  cfg.mesh = sanitize(m);
  cfg.meshKey = loadMeshKey();

  cfg.intervalSec = constrain(cfg.intervalSec, kMinIntervalSec, kMaxIntervalSec);
  cfg.loraIntervalSec = constrain(cfg.loraIntervalSec, kMinLoraIntervalSec, kMaxLoraIntervalSec);
  cfg.mode = mode <= (uint8_t)TrackerMode::Offline ? (TrackerMode)mode : TrackerMode::WifiOnly;
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
  MeshSettings m = sanitize(cfg.mesh);
  prefs.putUChar("preset", (uint8_t)m.preset);
  prefs.putUChar("slot", m.slot);
  prefs.putUChar("hops", m.hopLimit);
  prefs.putChar("txpower", m.txPowerDbm);
  prefs.putUChar("relay", (uint8_t)m.relayMode);
  prefs.putUChar("fairtime", m.foreignAirtimePct);
  if (validMeshKey(cfg.meshKey) && cfg.meshKey != kPlaceholderMeshKey) prefs.putString("meshkey", cfg.meshKey);
  prefs.end();
}

MeshSettings sanitize(MeshSettings s) {
  if ((uint8_t)s.preset >= (uint8_t)LoraPreset::Count) s.preset = LoraPreset::LongFast;
  if (s.slot > kMaxSlot) s.slot = 0;
  s.hopLimit = constrain(s.hopLimit, kMinHopLimit, kMaxHopLimit);
  s.txPowerDbm = constrain(s.txPowerDbm, kMinTxPowerDbm, kMaxTxPowerDbm);
  if ((uint8_t)s.relayMode > (uint8_t)RelayMode::None) s.relayMode = RelayMode::All;
  if (s.foreignAirtimePct > kMaxForeignAirtimePct) s.foreignAirtimePct = kMaxForeignAirtimePct;
  return s;
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
  case TrackerMode::Gateway: return TrackerMode::Offline;
  case TrackerMode::Offline: return TrackerMode::LoraOnly;
  default: return TrackerMode::WifiOnly;
  }
}

const char *modeName(TrackerMode mode) {
  switch (mode) {
  case TrackerMode::Gateway: return "Gateway";
  case TrackerMode::Offline: return "Offline";
  case TrackerMode::LoraOnly: return "Nur LoRa";
  default: return "Nur WLAN";
  }
}

const char *modeShort(TrackerMode mode) {
  switch (mode) {
  case TrackerMode::Gateway: return "GW";
  case TrackerMode::Offline: return "Off";
  case TrackerMode::LoraOnly: return "LoRa";
  default: return "WLAN";
  }
}

} // namespace config
