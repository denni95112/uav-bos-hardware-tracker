#pragma once

#include <Arduino.h>

#include "config.h"

namespace portal {

// Config access point with captive portal. Serves the settings page on 192.168.4.1.
void startAp(TrackerConfig &cfg, const String &apSsid);
// Settings/status page on the station IP while connected to the vehicle WiFi.
void startSta(TrackerConfig &cfg);
void stop();
void loop();

bool isApMode();
bool rebootRequested();   // set after settings were saved or reset
uint32_t lastActivityMs(); // last HTTP request or AP client connected
uint8_t apClients();

} // namespace portal
