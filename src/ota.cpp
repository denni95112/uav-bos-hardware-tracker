#include "ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "certs.h"
#include "display.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

#ifndef OTA_REPO
#define OTA_REPO "denni95112/uav-bos-hardware-tracker"
#endif

namespace ota {

namespace {

constexpr uint16_t kTimeoutMs = 10000;
constexpr uint8_t kMaxRedirects = 5;
constexpr int kErrTooManyRedirects = -100;
const char *kReleases = "https://github.com/" OTA_REPO "/releases";

bool parseVersion(String s, int v[3]) {
  s.trim();
  if (s.startsWith("v")) s.remove(0, 1);
  return sscanf(s.c_str(), "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

bool isNewer(const int remote[3], const int local[3]) {
  for (int i = 0; i < 3; i++) {
    if (remote[i] != local[i]) return remote[i] > local[i];
  }
  return false;
}

// GET with redirects followed by hand: release assets are served from a different host, and the
// connection must not be reused across hosts.
int open(HTTPClient &http, WiFiClientSecure &client, String url) {
  static const char *headers[] = {"Location"};
  http.setReuse(false);
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  http.setUserAgent("uav-bos-tracker/" FW_VERSION);

  for (uint8_t hop = 0; hop <= kMaxRedirects; hop++) {
    if (!url.startsWith("https://") || !http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
    http.collectHeaders(headers, 1);
    int code = http.GET();
    if (code < 300 || code >= 400 || !http.hasHeader("Location")) return code;
    url = http.header("Location");
    http.end();
  }
  return kErrTooManyRedirects;
}

// Returns false on network errors. An empty `version` means the latest release has no version.txt.
bool fetchLatestVersion(WiFiClientSecure &client, String &version) {
  HTTPClient http;
  int code = open(http, client, String(kReleases) + "/latest/download/version.txt");
  version = "";
  bool ok = true;
  if (code == HTTP_CODE_OK) {
    version = http.getString();
    version.trim();
  } else if (code != HTTP_CODE_NOT_FOUND) {
    Serial.printf("[ota] version check failed: %d %s\n", code, HTTPClient::errorToString(code).c_str());
    ok = false;
  }
  http.end();
  return ok;
}

void showProgress(const String &version, int percent) {
  char line1[32], line2[16];
  snprintf(line1, sizeof(line1), "Lade v%s", version.c_str());
  snprintf(line2, sizeof(line2), "%d %%", percent);
  display::showMessage("Firmware-Update", line1, line2);
}

// Returns only on failure; reboots after a successful install.
void install(WiFiClientSecure &client, const String &version) {
  String url = String(kReleases) + "/download/v" + version + "/uav-bos-tracker-" + version + ".bin";
  HTTPClient http;
  int code = open(http, client, url);
  int total = http.getSize();
  if (code != HTTP_CODE_OK || total <= 0) {
    Serial.printf("[ota] download failed: %d, size %d\n", code, total);
    http.end();
    return;
  }
  if (!Update.begin(total, U_FLASH)) {
    Serial.printf("[ota] %s\n", Update.errorString());
    http.end();
    return;
  }

  Serial.printf("[ota] installing %s (%d bytes)\n", url.c_str(), total);
  showProgress(version, 0);
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[2048];
  int written = 0;
  int lastPercent = 0;
  uint32_t lastDataMs = millis();
  while (written < total) {
    size_t avail = stream->available();
    if (avail == 0) {
      if (!stream->connected() || millis() - lastDataMs > kTimeoutMs) break;
      delay(1);
      continue;
    }
    int n = stream->readBytes(buf, min(avail, sizeof(buf)));
    if (n <= 0) continue;
    if (Update.write(buf, n) != (size_t)n) break;
    written += n;
    lastDataMs = millis();
    int percent = (int)((int64_t)written * 100 / total);
    if (percent >= lastPercent + 5) {
      lastPercent = percent;
      showProgress(version, percent);
    }
  }
  http.end();

  if (written != total || !Update.end()) {
    Serial.printf("[ota] install failed after %d/%d bytes: %s\n", written, total, Update.errorString());
    Update.abort();
    display::showMessage("Firmware-Update", "Fehlgeschlagen", "Alte Version bleibt");
    delay(2000);
    return;
  }

  Serial.println("[ota] installed, rebooting");
  display::showMessage("Firmware-Update", "Installiert", "Neustart...");
  delay(1500);
  ESP.restart();
}

} // namespace

bool checkAndInstall() {
  int local[3];
  if (!parseVersion(FW_VERSION, local)) {
    Serial.println("[ota] no release version in this build, skipping update check");
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) return false;

  // Without verification anyone on the vehicle WiFi could push firmware.
  WiFiClientSecure client;
  client.setCACert(kRootCas);

  String latest;
  if (!fetchLatestVersion(client, latest)) return false;

  int remote[3];
  if (!parseVersion(latest, remote)) {
    Serial.printf("[ota] no usable version in latest release ('%s')\n", latest.c_str());
    return true;
  }
  if (!isNewer(remote, local)) {
    Serial.printf("[ota] firmware %s is up to date (latest %s)\n", FW_VERSION, latest.c_str());
    return true;
  }

  String version = String(remote[0]) + "." + remote[1] + "." + remote[2];
  Serial.printf("[ota] update %s -> %s\n", FW_VERSION, version.c_str());
  install(client, version);
  return true;
}

} // namespace ota
