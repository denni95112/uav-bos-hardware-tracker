#pragma once

#include <Arduino.h>

#include "gnss.h"
#include "uplink.h"

namespace display {

void begin();
void setBacklight(bool on);
void toggleBacklight();

void showBoot(const char *version);
void showMessage(const char *title, const char *line1, const char *line2 = "");
void showConnecting(const String &ssid, uint32_t elapsedMs);
void showAp(const String &apSsid, const String &apPass, uint8_t clients);
void showRunning(const GnssFix &fix, const UplinkStatus &up, int rssi, const String &ip);

} // namespace display
