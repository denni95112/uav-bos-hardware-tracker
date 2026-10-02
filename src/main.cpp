#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "gateway.h"
#include "gnss.h"
#include "mesh.h"
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
constexpr uint32_t kBacklightHoldMs = 3000;
constexpr uint32_t kApHoldMs = 10000;
constexpr uint32_t kModeCommitMs = 3000;
constexpr uint32_t kDebounceMs = 40;
constexpr uint32_t kDisplayRefreshMs = 500;
constexpr uint32_t kConnectingRefreshMs = 250;

enum class State { Connecting, ConfigAp, Running, LoraOnly };
enum class Button { None, Short, HoldBacklight, HoldAp };

State state = State::Connecting;
uint32_t stateSinceMs = 0;
bool apAutoLeave = false; // AP was opened by a connect timeout, not by the user
TrackerConfig cfg;
String apSsid;
uint32_t lastSendMs = 0;
uint32_t lastDisplayMs = 0;

bool modeSelecting = false;
TrackerMode selectedMode = TrackerMode::WifiOnly;
uint32_t modeSelectMs = 0;

String makeApSsid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "UAV-BOS-Tracker-%02X%02X", mac[4], mac[5]);
  return String(buf);
}

// Everything the current mode needs to run without the config AP.
bool configReady() { return cfg.usesWifi() ? cfg.hasWifi() && cfg.hasUrl() : cfg.hasUrl(); }

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
  apAutoLeave = autoLeave && cfg.usesWifi() && cfg.hasWifi() && cfg.hasUrl();
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

void enterLoraOnly() {
  portal::stop();
  state = State::LoraOnly;
  stateSinceMs = millis();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[main] LoRa only, WiFi off");
}

void applyMode() {
  Serial.printf("[main] mode %s\n", config::modeName(cfg.mode));
  display::setMode(cfg.mode);
  if (cfg.usesLora()) mesh::begin();
  mesh::setActive(cfg.usesLora());

  if (!configReady()) enterConfigAp(false);
  else if (cfg.usesWifi()) enterConnecting();
  else enterLoraOnly();
}

void switchMode(TrackerMode mode) {
  if (mode == cfg.mode) return;
  bool wifiBefore = cfg.usesWifi();
  cfg.mode = mode;
  config::saveMode(mode);
  if (wifiBefore && cfg.usesWifi()) {
    // WLAN <-> Gateway only changes the radio, the WiFi connection (and the web page) stays up.
    Serial.printf("[main] mode %s\n", config::modeName(cfg.mode));
    display::setMode(cfg.mode);
    if (cfg.usesLora()) mesh::begin();
    mesh::setActive(cfg.usesLora());
  } else {
    applyMode();
  }
  lastDisplayMs = 0;
}

// Applied with a short delay so the HTTP response still reaches the browser before WiFi changes.
void handleWebModeRequest() {
  static bool pending = false;
  static TrackerMode mode;
  static uint32_t requestMs = 0;
  TrackerMode req;
  if (portal::takeModeRequest(req)) {
    pending = true;
    mode = req;
    requestMs = millis();
  }
  if (pending && millis() - requestMs >= 500) {
    pending = false;
    modeSelecting = false;
    switchMode(mode);
  }
}

Button pollButton() {
  static bool lastRaw = false;
  static bool pressed = false;
  static uint8_t holdFired = 0; // 0 none, 1 backlight, 2 AP
  static uint32_t changedMs = 0;
  static uint32_t pressedMs = 0;

  bool raw = digitalRead(PIN_BUTTON) == LOW;
  uint32_t now = millis();
  if (raw != lastRaw) {
    lastRaw = raw;
    changedMs = now;
  }
  if (now - changedMs < kDebounceMs) return Button::None;

  if (raw && !pressed) {
    pressed = true;
    holdFired = 0;
    pressedMs = now;
  } else if (raw && pressed) {
    uint32_t held = now - pressedMs;
    if (holdFired < 2 && held >= kApHoldMs) {
      holdFired = 2;
      return Button::HoldAp;
    }
    if (holdFired < 1 && held >= kBacklightHoldMs) {
      holdFired = 1;
      return Button::HoldBacklight;
    }
  } else if (!raw && pressed) {
    pressed = false;
    if (!holdFired) return Button::Short;
  }
  return Button::None;
}

void handleButton() {
  switch (pollButton()) {
  case Button::Short:
    display::setBacklight(true);
    selectedMode = config::nextMode(modeSelecting ? selectedMode : cfg.mode);
    modeSelecting = true;
    modeSelectMs = millis();
    lastDisplayMs = 0;
    break;
  case Button::HoldBacklight:
    modeSelecting = false;
    display::toggleBacklight();
    break;
  case Button::HoldAp:
    modeSelecting = false;
    display::setBacklight(true);
    if (state == State::ConfigAp) {
      if (configReady()) applyMode();
    } else {
      enterConfigAp(false);
    }
    break;
  default:
    break;
  }

  if (modeSelecting && millis() - modeSelectMs >= kModeCommitMs) {
    modeSelecting = false;
    lastDisplayMs = 0;
    switchMode(selectedMode);
  }
}

void checkReboot() {
  if (!portal::rebootRequested()) return;
  display::showMessage("Gespeichert", "Neustart...");
  delay(1500);
  ESP.restart();
}

void loopConnecting() {
  if (WiFi.status() == WL_CONNECTED) {
    enterRunning();
    return;
  }
  if (millis() - stateSinceMs > kConnectTimeoutMs) {
    Serial.println("[main] connect timeout, opening config AP");
    enterConfigAp(true);
  }
}

void loopConfigAp() {
  portal::loop();
  checkReboot();

  if (apAutoLeave && portal::apClients() == 0 && WiFi.status() == WL_CONNECTED) {
    Serial.println("[main] vehicle WiFi available again, leaving AP");
    portal::stop();
    enterRunning();
    return;
  }

  if (configReady() && millis() - portal::lastActivityMs() > kApIdleTimeoutMs) {
    Serial.println("[main] AP idle timeout, leaving AP");
    applyMode();
  }
}

void loopRunning(const GnssFix &fix) {
  portal::loop();
  checkReboot();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[main] WiFi lost");
    enterConnecting();
    return;
  }

  uint32_t now = millis();
  if (fix.valid && (lastSendMs == 0 || now - lastSendMs >= uplink::nextDelayMs(cfg.intervalSec))) {
    lastSendMs = now;
    uplink::send(fix);
  }
}

void loopMesh(const GnssFix &fix) {
  if (!cfg.usesLora()) return;
  bool wifiUp = state == State::Running && WiFi.status() == WL_CONNECTED;
  // A gateway without internet falls back to sending its own position over LoRa.
  bool sendOwn = cfg.mode == TrackerMode::LoraOnly || !wifiUp;
  mesh::loop(fix, cfg.url, cfg.loraIntervalSec, sendOwn);
  gateway::loop(cfg.mode == TrackerMode::Gateway && wifiUp, fix);
}

void updateDisplay(const GnssFix &fix) {
  uint32_t now = millis();
  uint32_t refresh = state == State::Connecting && !modeSelecting ? kConnectingRefreshMs : kDisplayRefreshMs;
  if (lastDisplayMs && now - lastDisplayMs < refresh) return;
  lastDisplayMs = now;

  if (modeSelecting) {
    display::showModeSelect(selectedMode, cfg.mode, kModeCommitMs - min(kModeCommitMs, now - modeSelectMs));
    return;
  }

  MeshStats ms = mesh::stats();
  switch (state) {
  case State::Connecting:
    display::showConnecting(cfg.wifiSsid, now - stateSinceMs);
    break;
  case State::ConfigAp:
    display::showAp(apSsid, cfg.apPass.length() >= 8 ? cfg.apPass : String(""), portal::apClients());
    break;
  case State::Running:
    display::showRunning(fix, uplink::status(), WiFi.RSSI(), WiFi.localIP().toString(),
                         cfg.mode == TrackerMode::Gateway ? &ms : nullptr,
                         cfg.mode == TrackerMode::Gateway ? &gateway::stats() : nullptr);
    break;
  case State::LoraOnly:
    display::showLoraOnly(fix, ms, cfg.loraIntervalSec);
    break;
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
  gateway::begin();

  WiFi.persistent(false);
  WiFi.setHostname("uav-bos-tracker");
  WiFi.mode(WIFI_STA);
  apSsid = makeApSsid();

  Serial.printf("\n[main] UAV-BOS tracker %s, AP name %s\n", FW_VERSION, apSsid.c_str());
  delay(kBootScreenMs);

  applyMode();
}

void loop() {
  handleButton();
  handleWebModeRequest();
  GnssFix fix = gnss::current();
  switch (state) {
  case State::Connecting: loopConnecting(); break;
  case State::ConfigAp: loopConfigAp(); break;
  case State::Running: loopRunning(fix); break;
  case State::LoraOnly: break;
  }
  loopMesh(fix);
  updateDisplay(fix);
  delay(2);
}
