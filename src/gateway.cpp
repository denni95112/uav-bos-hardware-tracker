#include "gateway.h"

#include <Preferences.h>

#include "mesh.h"

namespace gateway {

namespace {

constexpr size_t kCacheSize = 32;
constexpr uint32_t kMaxPositionAgeSec = 60;
constexpr uint32_t kCredRequestGapMs = 60000;
const char *kNamespace = "meshcache";

struct CacheEntry {
  uint32_t node = 0;
  uint16_t hash = 0;
  String url;
  uint32_t lastUsedMs = 0;
  uint32_t lastRequestMs = 0; // credential request sent, 0 = never
};

CacheEntry cache[kCacheSize];
GatewayStats st;

String nvsKey(uint32_t node) {
  char buf[12];
  snprintf(buf, sizeof(buf), "u%08lx", (unsigned long)node);
  return String(buf);
}

void countKnown() {
  uint8_t n = 0;
  for (const CacheEntry &e : cache) {
    if (e.node && e.url.length()) n++;
  }
  st.knownNodes = n;
}

void saveIndex(Preferences &prefs) {
  uint32_t nodes[kCacheSize];
  for (size_t i = 0; i < kCacheSize; i++) nodes[i] = cache[i].url.length() ? cache[i].node : 0;
  prefs.putBytes("nodes", nodes, sizeof(nodes));
}

CacheEntry *find(uint32_t node) {
  for (CacheEntry &e : cache) {
    if (e.node == node) return &e;
  }
  return nullptr;
}

// Returns the entry for `node`, reusing the least recently used slot if needed.
CacheEntry &findOrCreate(uint32_t node) {
  if (CacheEntry *e = find(node)) return *e;
  CacheEntry *victim = &cache[0];
  for (CacheEntry &e : cache) {
    if (!e.node) {
      victim = &e;
      break;
    }
    if (e.lastUsedMs < victim->lastUsedMs) victim = &e;
  }
  if (victim->node && victim->url.length()) {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.remove(nvsKey(victim->node).c_str());
    prefs.end();
  }
  *victim = CacheEntry();
  victim->node = node;
  return *victim;
}

void storeCredentials(const MeshRx &rx) {
  CacheEntry &e = findOrCreate(rx.from);
  e.lastUsedMs = millis();
  if (e.hash == rx.urlHash && e.url == rx.url) return;
  e.hash = rx.urlHash;
  e.url = rx.url;

  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString(nvsKey(rx.from).c_str(), e.url);
  saveIndex(prefs);
  prefs.end();
  countKnown();
  Serial.printf("[gateway] credentials for !%08lx cached\n", (unsigned long)rx.from);
}

void askForCredentials(uint32_t node) {
  CacheEntry &e = findOrCreate(node);
  uint32_t now = millis();
  if (e.lastRequestMs && now - e.lastRequestMs < kCredRequestGapMs) return;
  e.lastRequestMs = now;
  if (!e.lastUsedMs) e.lastUsedMs = now;
  mesh::requestCredentials(node);
  Serial.printf("[gateway] requesting credentials from !%08lx\n", (unsigned long)node);
}

// Returns true if an HTTP request was made.
bool forwardPosition(const MeshRx &rx, const GnssFix &ownFix) {
  const meshproto::Position &p = rx.pos;
  if (ownFix.unixTime && p.fixTime && ownFix.unixTime > p.fixTime + kMaxPositionAgeSec) {
    st.dropped++;
    return false;
  }

  CacheEntry *e = find(rx.from);
  if (!e || !e->url.length() || e->hash != p.urlHash) {
    st.dropped++;
    askForCredentials(rx.from);
    return false;
  }
  e->lastUsedMs = millis();

  GnssFix f;
  f.valid = true;
  f.latitude = p.latitude;
  f.longitude = p.longitude;
  f.altitude = p.altitude;
  f.speedMps = p.speedMps;
  f.heading = p.heading;
  f.accuracy = p.accuracy;

  if (uplink::post(e->url, uplink::buildJson(f), st.up)) {
    st.forwarded++;
  } else {
    st.failed++;
  }
  Serial.printf("[gateway] !%08lx (%u hops, SNR %.1f) -> HTTP %d\n", (unsigned long)rx.from, rx.hops, rx.snr,
                st.up.lastHttpCode);
  return true;
}

} // namespace

void begin() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  uint32_t nodes[kCacheSize] = {0};
  if (prefs.getBytesLength("nodes") == sizeof(nodes)) prefs.getBytes("nodes", nodes, sizeof(nodes));
  for (size_t i = 0; i < kCacheSize; i++) {
    if (!nodes[i]) continue;
    String url = prefs.getString(nvsKey(nodes[i]).c_str(), "");
    if (!url.length()) continue;
    cache[i].node = nodes[i];
    cache[i].url = url;
    cache[i].hash = meshproto::urlHash(std::string(url.c_str()));
  }
  prefs.end();
  countKnown();
  Serial.printf("[gateway] %u cached client URLs\n", st.knownNodes);
}

void loop(bool forward, const GnssFix &ownFix) {
  MeshRx rx;
  while (mesh::receive(rx)) {
    if (rx.type == meshproto::MsgCredentials) {
      storeCredentials(rx);
    } else if (rx.type == meshproto::MsgPosition) {
      if (!forward) continue;
      if (forwardPosition(rx, ownFix)) return;
    }
  }
}

const GatewayStats &stats() { return st; }

} // namespace gateway
