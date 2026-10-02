#include "gnss.h"

#include <TinyGPSPlus.h>
#include <math.h>

#include "pins.h"

namespace gnss {

// Below this speed the GNSS course over ground is mostly noise.
static constexpr float kHeadingMinSpeedMps = 1.0f;
// User equivalent range error used to turn HDOP into metres.
static constexpr float kUereMeters = 2.5f;
static constexpr uint32_t kGstMaxAgeMs = 3000;

static HardwareSerial gnssSerial(1);
static TinyGPSPlus gps;

// $--GST fields 6/7: standard deviation of latitude / longitude error in metres.
static TinyGPSCustom gnGstLat(gps, "GNGST", 6);
static TinyGPSCustom gnGstLon(gps, "GNGST", 7);
static TinyGPSCustom gpGstLat(gps, "GPGST", 6);
static TinyGPSCustom gpGstLon(gps, "GPGST", 7);

static float heldHeading = 0;
static SemaphoreHandle_t gpsMutex = nullptr;

// Runs on its own task so blocking HTTP requests in the main loop cannot overflow the UART buffer.
static void gnssTask(void *) {
  for (;;) {
    if (gnssSerial.available()) {
      xSemaphoreTake(gpsMutex, portMAX_DELAY);
      while (gnssSerial.available()) gps.encode(gnssSerial.read());
      xSemaphoreGive(gpsMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void begin() {
  gpsMutex = xSemaphoreCreateMutex();

  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, VEXT_ON);

  pinMode(PIN_GNSS_RST, OUTPUT);
  digitalWrite(PIN_GNSS_RST, LOW);
  delay(20);
  digitalWrite(PIN_GNSS_RST, HIGH);

  gnssSerial.setRxBufferSize(2048);
  gnssSerial.begin(GNSS_BAUD, SERIAL_8N1, PIN_GNSS_RX, PIN_GNSS_TX);
  delay(100);

  // Enable GPS L1/L5, BDS, GLONASS, Galileo and QZSS (same setting Meshtastic uses for the UC6580).
  gnssSerial.print("$CFGSYS,h35155\r\n");

  xTaskCreatePinnedToCore(gnssTask, "gnss", 4096, nullptr, 2, nullptr, 0);
}

static bool gstAccuracy(float &out) {
  TinyGPSCustom *lat = nullptr;
  TinyGPSCustom *lon = nullptr;
  if (gnGstLat.isValid() && gnGstLat.age() < kGstMaxAgeMs) {
    lat = &gnGstLat;
    lon = &gnGstLon;
  } else if (gpGstLat.isValid() && gpGstLat.age() < kGstMaxAgeMs) {
    lat = &gpGstLat;
    lon = &gpGstLon;
  }
  if (!lat || !lon->isValid()) return false;

  const char *latStr = lat->value();
  const char *lonStr = lon->value();
  if (!latStr[0] || !lonStr[0]) return false;

  float la = atof(latStr);
  float lo = atof(lonStr);
  float acc = sqrtf(la * la + lo * lo);
  if (acc <= 0 || acc > 10000) return false;
  out = acc;
  return true;
}

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
static int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

static uint32_t unixTime() {
  if (!gps.date.isValid() || !gps.time.isValid() || gps.date.year() < 2024) return 0;
  int32_t days = daysFromCivil(gps.date.year(), gps.date.month(), gps.date.day());
  return (uint32_t)days * 86400UL + gps.time.hour() * 3600UL + gps.time.minute() * 60UL + gps.time.second();
}

static GnssFix snapshot() {
  GnssFix f;
  f.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
  f.hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 0;
  f.unixTime = unixTime();

  if (!gps.location.isValid()) return f;

  f.ageMs = gps.location.age();
  f.latitude = gps.location.lat();
  f.longitude = gps.location.lng();
  f.altitude = gps.altitude.isValid() ? gps.altitude.meters() : 0;
  f.speedMps = gps.speed.isValid() ? gps.speed.mps() : 0;

  if (gps.course.isValid() && f.speedMps >= kHeadingMinSpeedMps) {
    heldHeading = gps.course.deg();
  }
  f.heading = heldHeading;

  float acc;
  if (gstAccuracy(acc)) {
    f.accuracy = acc;
    f.accuracyFromGst = true;
  } else {
    f.accuracy = f.hdop > 0 ? f.hdop * kUereMeters : 99.0f;
  }

  f.valid = f.ageMs < kMaxFixAgeMs;
  return f;
}

GnssFix current() {
  xSemaphoreTake(gpsMutex, portMAX_DELAY);
  GnssFix f = snapshot();
  xSemaphoreGive(gpsMutex);
  return f;
}

uint32_t charsProcessed() {
  xSemaphoreTake(gpsMutex, portMAX_DELAY);
  uint32_t n = gps.charsProcessed();
  xSemaphoreGive(gpsMutex);
  return n;
}

} // namespace gnss
