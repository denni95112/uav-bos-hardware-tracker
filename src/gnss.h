#pragma once

#include <Arduino.h>

struct GnssFix {
  bool valid = false;    // location valid and fresh
  double latitude = 0;
  double longitude = 0;
  float altitude = 0;    // m above MSL
  float speedMps = 0;    // m/s
  float heading = 0;     // deg, 0..360, held while standing still
  float accuracy = 0;    // m, horizontal estimate
  float hdop = 0;
  uint8_t satellites = 0;
  uint32_t ageMs = UINT32_MAX;
  bool accuracyFromGst = false;
};

namespace gnss {

// Maximum age of a location to be considered usable for sending.
constexpr uint32_t kMaxFixAgeMs = 2000;

void begin(); // powers the module and starts the NMEA parser task
GnssFix current();
uint32_t charsProcessed();

} // namespace gnss
