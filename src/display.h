#pragma once

#include <Arduino.h>

#include "config.h"
#include "gateway.h"
#include "gnss.h"
#include "mesh.h"
#include "uplink.h"

namespace display {

void begin();
void setBacklight(bool on);
void toggleBacklight();
bool backlightOn();
void setMode(TrackerMode mode); // badge shown in the header of every screen

void showBoot(const char *version);
void showMessage(const char *title, const char *line1, const char *line2 = "");
void showConnecting(const String &ssid, uint32_t elapsedMs);
void showAp(const String &apSsid, const String &apPass, uint8_t clients);
// `mesh` and `gw` are only passed in Gateway mode.
void showRunning(const GnssFix &fix, const UplinkStatus &up, int rssi, const String &ip, const MeshStats *mesh,
                 const GatewayStats *gw);
void showLoraOnly(const GnssFix &fix, const MeshStats &mesh, uint16_t intervalSec);
void showModeSelect(TrackerMode selected, TrackerMode current, uint32_t remainingMs);

} // namespace display
