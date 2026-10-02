#include "uplink.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "certs.h"

namespace uplink {

static constexpr uint16_t kTimeoutMs = 5000;
static constexpr uint32_t kMaxBackoffMs = 60000;

static String targetUrl;
static WiFiClientSecure secureClient;
static WiFiClient plainClient;
static HTTPClient http;
static UplinkStatus st;

void begin(const String &url) {
  targetUrl = url;
  targetUrl.trim();
  // The URL carries the API key, so the server must be authenticated. Servers outside the bundle need
  // -DUPLINK_INSECURE_TLS.
#ifdef UPLINK_INSECURE_TLS
  secureClient.setInsecure();
#else
  secureClient.setCACert(kRootCas);
#endif
  http.setReuse(true);
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
}

String buildJson(const GnssFix &fix) {
  char buf[200];
  snprintf(buf, sizeof(buf),
           "{\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.1f,\"speed\":%.1f,\"heading\":%.0f,"
           "\"accuracy\":%.1f}",
           fix.latitude, fix.longitude, fix.altitude, fix.speedMps, fix.heading, fix.accuracy);
  return String(buf);
}

bool send(const GnssFix &fix) { return post(targetUrl, buildJson(fix), st); }

bool post(const String &url, const String &body, UplinkStatus &st) {
  st.lastAttemptMs = millis();
  if (st.lastAttemptMs == 0) st.lastAttemptMs = 1;

  if (WiFi.status() != WL_CONNECTED) {
    st.lastHttpCode = HTTPC_ERROR_NOT_CONNECTED;
    st.lastError = "WLAN nicht verbunden";
    st.failCount++;
    st.consecutiveFails++;
    return false;
  }

  st.lastPayload = body;

  // A kept-alive connection must not be reused for a different server.
  static String lastOrigin;
  int pathStart = url.indexOf('/', url.indexOf("://") + 3);
  String origin = pathStart > 0 ? url.substring(0, pathStart) : url;
  if (origin != lastOrigin) {
    secureClient.stop();
    plainClient.stop();
    lastOrigin = origin;
  }

  bool ok = url.startsWith("https://") ? http.begin(secureClient, url) : http.begin(plainClient, url);
  if (!ok) {
    st.lastHttpCode = HTTPC_ERROR_CONNECTION_REFUSED;
    st.lastError = "Ungueltige URL";
    st.failCount++;
    st.consecutiveFails++;
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  st.lastHttpCode = code;

  bool success = code >= 200 && code < 300;
  if (success) {
    st.lastSuccessMs = st.lastAttemptMs;
    st.sentCount++;
    st.consecutiveFails = 0;
    st.lastError = "";
    http.getString(); // drain the response so the connection can be reused
  } else {
    st.failCount++;
    st.consecutiveFails++;
    if (code > 0) {
      st.lastError = http.getString();
      if (st.lastError.length() > 120) st.lastError = st.lastError.substring(0, 120);
    } else {
      st.lastError = HTTPClient::errorToString(code);
    }
  }
  http.end();

  Serial.printf("[uplink] %s -> %d %s\n", body.c_str(), code, st.lastError.c_str());
  return success;
}

const UplinkStatus &status() { return st; }

uint32_t nextDelayMs(uint16_t intervalSec) {
  uint32_t base = (uint32_t)intervalSec * 1000;
  if (st.consecutiveFails == 0) return base;
  uint32_t d = base << min<uint16_t>(st.consecutiveFails, 6);
  return max(base, min(d, kMaxBackoffMs));
}

} // namespace uplink
