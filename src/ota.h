#pragma once

#include <Arduino.h>

namespace ota {

// Asks GitHub for the latest release and installs it if it is newer than FW_VERSION. Blocks while
// downloading and reboots after a successful install. Returns false if the check could not be completed
// (no internet, timeout) so the caller can retry; true if the firmware is up to date or no release exists.
bool checkAndInstall();

} // namespace ota
