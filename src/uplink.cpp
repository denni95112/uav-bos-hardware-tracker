#include "uplink.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace uplink {

static constexpr uint16_t kTimeoutMs = 5000;
static constexpr uint32_t kMaxBackoffMs = 60000;

#ifdef UPLINK_VERIFY_TLS
// ISRG Root X1 (Let's Encrypt), root of api.beta.uav-bos.de. Valid until 2035-06-04.
static const char *kRootCa = R"PEM(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)PEM";
#endif

static String targetUrl;
static WiFiClientSecure secureClient;
static WiFiClient plainClient;
static HTTPClient http;
static UplinkStatus st;

void begin(const String &url) {
  targetUrl = url;
  targetUrl.trim();
#ifdef UPLINK_VERIFY_TLS
  secureClient.setCACert(kRootCa);
#else
  secureClient.setInsecure();
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

bool send(const GnssFix &fix) {
  st.lastAttemptMs = millis();
  if (st.lastAttemptMs == 0) st.lastAttemptMs = 1;

  if (WiFi.status() != WL_CONNECTED) {
    st.lastHttpCode = HTTPC_ERROR_NOT_CONNECTED;
    st.lastError = "WLAN nicht verbunden";
    st.failCount++;
    st.consecutiveFails++;
    return false;
  }

  String body = buildJson(fix);
  st.lastPayload = body;

  bool ok = targetUrl.startsWith("https://") ? http.begin(secureClient, targetUrl)
                                              : http.begin(plainClient, targetUrl);
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
