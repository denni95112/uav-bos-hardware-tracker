#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "gnss.h"
#include "pins.h"
#include "portal.h"
#include "uplink.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

namespace {

constexpr uint32_t kBootScreenMs = 1500;
constexpr uint32_t kConnectTimeoutMs = 30000;
constexpr uint32_t kApIdleTimeoutMs = 5UL * 60 * 1000;
constexpr uint32_t kLongPressMs = 3000;
constexpr uint32_t kDebounceMs = 40;
constexpr uint32_t kDisplayRefreshMs = 500;

enum class State { Connecting, ConfigAp, Running };

State state = State::Connecting;
uint32_t stateSinceMs = 0;
bool apAutoLeave = false; // AP was opened by a connect timeout, not by the user
TrackerConfig cfg;
String apSsid;
uint32_t lastSendMs = 0;
uint32_t lastDisplayMs = 0;

String makeApSsid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "UAV-BOS-Tracker-%02X%02X", mac[4], mac[5]);
  return String(buf);
}

void enterConnecting() {
  portal::stop();
  state = State::Connecting;
  stateSinceMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  Serial.printf("[main] connecting to %s\n", cfg.wifiSsid.c_str());
}

void enterConfigAp(bool autoLeave) {
  state = State::ConfigAp;
  stateSinceMs = millis();
  apAutoLeave = autoLeave && cfg.isComplete();
  portal::startAp(cfg, apSsid);
  // Keep trying the vehicle WiFi in the background so the tracker recovers on its own after a timeout.
  if (apAutoLeave) WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
}

void enterRunning() {
  state = State::Running;
  stateSinceMs = millis();
  WiFi.mode(WIFI_STA);
  portal::startSta(cfg);
  lastSendMs = 0;
  Serial.printf("[main] connected, IP %s, RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// Returns 1 for a short press (on release), 2 once when a long press is reached, 0 otherwise.
int pollButton() {
  static bool lastRaw = false;
  static bool pressed = false;
  static bool longFired = false;
  static uint32_t changedMs = 0;
  static uint32_t pressedMs = 0;

  bool raw = digitalRead(PIN_BUTTON) == LOW;
  uint32_t now = millis();
  if (raw != lastRaw) {
    lastRaw = raw;
    changedMs = now;
  }
  if (now - changedMs < kDebounceMs) return 0;

  if (raw && !pressed) {
    pressed = true;
    longFired = false;
    pressedMs = now;
  } else if (raw && pressed && !longFired && now - pressedMs >= kLongPressMs) {
    longFired = true;
    return 2;
  } else if (!raw && pressed) {
    pressed = false;
    if (!longFired) return 1;
  }
  return 0;
}

void handleButton() {
  switch (pollButton()) {
  case 1:
    display::toggleBacklight();
    break;
  case 2:
    display::setBacklight(true);
    if (state == State::ConfigAp) {
      if (cfg.isComplete()) enterConnecting();
    } else {
      enterConfigAp(false);
    }
    break;
  default:
    break;
  }
}

void loopConnecting() {
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    enterRunning();
    return;
  }
  if (now - stateSinceMs > kConnectTimeoutMs) {
    Serial.println("[main] connect timeout, opening config AP");
    enterConfigAp(true);
    return;
  }
  if (now - lastDisplayMs >= 250) {
    lastDisplayMs = now;
    display::showConnecting(cfg.wifiSsid, now - stateSinceMs);
  }
}

void loopConfigAp() {
  uint32_t now = millis();
  portal::loop();

  if (portal::rebootRequested()) {
    display::showMessage("Gespeichert", "Neustart...");
    delay(1500);
    ESP.restart();
  }

  if (apAutoLeave && portal::apClients() == 0 && WiFi.status() == WL_CONNECTED) {
    Serial.println("[main] vehicle WiFi available again, leaving AP");
    portal::stop();
    enterRunning();
    return;
  }

  if (cfg.isComplete() && now - portal::lastActivityMs() > kApIdleTimeoutMs) {
    Serial.println("[main] AP idle timeout, retrying WiFi");
    enterConnecting();
    return;
  }

  if (now - lastDisplayMs >= kDisplayRefreshMs) {
    lastDisplayMs = now;
    display::showAp(apSsid, cfg.apPass.length() >= 8 ? cfg.apPass : String(""), portal::apClients());
  }
}

void loopRunning() {
  uint32_t now = millis();
  portal::loop();

  if (portal::rebootRequested()) {
    display::showMessage("Gespeichert", "Neustart...");
    delay(1500);
    ESP.restart();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[main] WiFi lost");
    enterConnecting();
    return;
  }

  GnssFix fix = gnss::current();
  if (fix.valid && (lastSendMs == 0 || now - lastSendMs >= uplink::nextDelayMs(cfg.intervalSec))) {
    lastSendMs = now;
    uplink::send(fix);
    now = millis();
  }

  if (now - lastDisplayMs >= kDisplayRefreshMs) {
    lastDisplayMs = now;
    display::showRunning(fix, uplink::status(), WiFi.RSSI(), WiFi.localIP().toString());
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  display::begin();
  display::showBoot(FW_VERSION);
  gnss::begin();

  config::load(cfg);
  uplink::begin(cfg.url);

  WiFi.persistent(false);
  WiFi.setHostname("uav-bos-tracker");
  WiFi.mode(WIFI_STA);
  apSsid = makeApSsid();

  Serial.printf("\n[main] UAV-BOS tracker %s, AP name %s\n", FW_VERSION, apSsid.c_str());
  delay(kBootScreenMs);

  if (cfg.isComplete()) enterConnecting();
  else enterConfigAp(false);
}

void loop() {
  handleButton();
  switch (state) {
  case State::Connecting: loopConnecting(); break;
  case State::ConfigAp: loopConfigAp(); break;
  case State::Running: loopRunning(); break;
  }
  delay(2);
}
