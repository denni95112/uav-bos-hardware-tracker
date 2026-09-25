#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#include "pins.h"

namespace display {

namespace {

// Exposes the protected window offset setter; the Heltec panel is not centred like the stock mini 160x80.
class TrackerTft : public Adafruit_ST7735 {
public:
  using Adafruit_ST7735::Adafruit_ST7735;
  void setOffsets(int8_t col, int8_t row) { setColRowStart(col, row); }
};

constexpr int16_t W = 160;
constexpr int16_t H = 80;

constexpr uint16_t C_BG = ST77XX_BLACK;
constexpr uint16_t C_FG = ST77XX_WHITE;
constexpr uint16_t C_DIM = 0x8410;    // grey
constexpr uint16_t C_HEAD = 0xB800;   // fire red
constexpr uint16_t C_OK = 0x07E0;     // green
constexpr uint16_t C_WARN = 0xFE00;   // amber
constexpr uint16_t C_ERR = ST77XX_RED;
constexpr uint16_t C_INFO = 0x04FF;   // blue

TrackerTft tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
GFXcanvas16 canvas(W, H);
bool backlight = true;

void push() { tft.drawRGBBitmap(0, 0, canvas.getBuffer(), W, H); }

void header(const char *title, uint16_t color) {
  canvas.fillRect(0, 0, W, 13, color);
  canvas.setTextSize(1);
  canvas.setTextColor(C_FG);
  canvas.setCursor(3, 3);
  canvas.print(title);
}

void line(int16_t y, uint16_t color, const char *text) {
  canvas.setTextColor(color);
  canvas.setCursor(3, y);
  canvas.print(text);
}

void centered(int16_t y, uint8_t size, uint16_t color, const char *text) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.setTextSize(size);
  canvas.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  canvas.setTextColor(color);
  canvas.setCursor((W - (int16_t)w) / 2, y);
  canvas.print(text);
  canvas.setTextSize(1);
}

void formatAge(char *buf, size_t len, uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 120) snprintf(buf, len, "%lus", (unsigned long)s);
  else if (s < 7200) snprintf(buf, len, "%lum", (unsigned long)(s / 60));
  else snprintf(buf, len, "%luh", (unsigned long)(s / 3600));
}

} // namespace

void begin() {
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, VEXT_ON);
  delay(50);

  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  SPI.begin(PIN_TFT_SCLK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.initR(INITR_MINI160x80);
  tft.setOffsets(TFT_COL_OFFSET, TFT_ROW_OFFSET);
  tft.setRotation(TFT_ROTATION);
  tft.invertDisplay(TFT_INVERT);
  tft.setSPISpeed(40000000);
  tft.fillScreen(C_BG);

  canvas.setTextWrap(false);
}

void setBacklight(bool on) {
  backlight = on;
  digitalWrite(PIN_TFT_BL, on ? HIGH : LOW);
}

void toggleBacklight() { setBacklight(!backlight); }

void showBoot(const char *version) {
  canvas.fillScreen(C_BG);
  canvas.fillRect(0, 0, W, 4, C_HEAD);
  canvas.fillRect(0, H - 4, W, 4, C_HEAD);
  centered(14, 2, C_FG, "UAV-BOS");
  centered(36, 1, C_FG, "Fahrzeug-Tracker");
  char buf[24];
  snprintf(buf, sizeof(buf), "FW %s", version);
  centered(52, 1, C_DIM, buf);
  centered(64, 1, C_DIM, "Starte...");
  push();
}

void showMessage(const char *title, const char *line1, const char *line2) {
  canvas.fillScreen(C_BG);
  header(title, C_INFO);
  centered(32, 1, C_FG, line1);
  centered(48, 1, C_DIM, line2);
  push();
}

void showConnecting(const String &ssid, uint32_t elapsedMs) {
  static const char spinner[] = "|/-\\";
  canvas.fillScreen(C_BG);
  header("WLAN verbinden", C_INFO);
  line(22, C_DIM, "Netzwerk:");
  line(34, C_FG, ssid.c_str());
  char buf[32];
  snprintf(buf, sizeof(buf), "%c  %lus", spinner[(elapsedMs / 250) % 4], (unsigned long)(elapsedMs / 1000));
  line(52, C_WARN, buf);
  line(66, C_DIM, "Lang druecken: Config-AP");
  push();
}

void showAp(const String &apSsid, const String &apPass, uint8_t clients) {
  canvas.fillScreen(C_BG);
  header("Konfiguration (AP)", C_WARN);
  line(18, C_DIM, "WLAN:");
  canvas.setCursor(36, 18);
  canvas.setTextColor(C_FG);
  canvas.print(apSsid);

  line(30, C_DIM, "PW:");
  canvas.setCursor(36, 30);
  canvas.setTextColor(C_FG);
  canvas.print(apPass.length() ? apPass.c_str() : "(offen)");

  line(42, C_DIM, "URL:");
  canvas.setCursor(36, 42);
  canvas.setTextColor(C_OK);
  canvas.print("http://192.168.4.1");

  char buf[32];
  snprintf(buf, sizeof(buf), "Verbundene Geraete: %u", clients);
  line(58, clients ? C_OK : C_DIM, buf);
  line(69, C_DIM, "Seite oeffnet automatisch");
  push();
}

void showRunning(const GnssFix &fix, const UplinkStatus &up, int rssi, const String &ip) {
  char buf[48];
  canvas.fillScreen(C_BG);

  snprintf(buf, sizeof(buf), "UAV-BOS   WLAN %ddBm", rssi);
  header(buf, C_HEAD);

  // GNSS status
  uint16_t gpsColor = fix.valid ? C_OK : (fix.ageMs != UINT32_MAX ? C_WARN : C_ERR);
  const char *gpsState = fix.valid ? "FIX" : (fix.ageMs != UINT32_MAX ? "ALT" : "SUCHE");
  snprintf(buf, sizeof(buf), "GPS %-5s Sat %2u HDOP %.1f", gpsState, fix.satellites, fix.hdop);
  line(16, gpsColor, buf);

  if (fix.ageMs != UINT32_MAX) {
    snprintf(buf, sizeof(buf), "%.6f  %.6f", fix.latitude, fix.longitude);
    line(27, C_FG, buf);
    snprintf(buf, sizeof(buf), "%3.0fkm/h %3.0f\xF7 %4.0fm +-%.0fm", fix.speedMps * 3.6f, fix.heading,
             fix.altitude, fix.accuracy);
    line(38, C_FG, buf);
  } else {
    line(27, C_DIM, "Warte auf Satelliten...");
    line(38, C_DIM, "Antenne frei zum Himmel?");
  }

  // Uplink status
  char age[12];
  if (up.lastAttemptMs == 0) {
    line(52, C_DIM, "Senden: noch nicht");
  } else {
    formatAge(age, sizeof(age), millis() - up.lastAttemptMs);
    bool ok = up.lastHttpCode >= 200 && up.lastHttpCode < 300;
    if (up.lastHttpCode > 0) snprintf(buf, sizeof(buf), "Senden: vor %s HTTP %d", age, up.lastHttpCode);
    else snprintf(buf, sizeof(buf), "Senden: vor %s FEHLER", age);
    line(52, ok ? C_OK : C_ERR, buf);
  }

  snprintf(buf, sizeof(buf), "OK %lu ERR %lu", (unsigned long)up.sentCount, (unsigned long)up.failCount);
  line(63, C_DIM, buf);
  line(72, C_DIM, ip.c_str());

  push();
}

} // namespace display
