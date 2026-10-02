#include "mesh.h"

#include <RadioLib.h>
#include <SPI.h>
#include <atomic>
#include <math.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

#include "pins.h"

#ifndef MESH_CHANNEL_NAME
#define MESH_CHANNEL_NAME "UAV-BOS"
#endif
#ifndef MESH_PSK_B64
#define MESH_PSK_B64 "CHANGE+ME+CHANGE+ME+CHANGE+ME+CHANGE+ME+CEE="
#endif

namespace mesh {

namespace {

using namespace meshproto;

const char kPlaceholderKey[] = "CHANGE+ME+CHANGE+ME+CHANGE+ME+CHANGE+ME+CEE=";

// Meshtastic EU_868 LongFast. The 869.4-869.65 MHz sub-band has room for exactly one 250 kHz channel.
constexpr float kFreqMhz = 869.525f;
constexpr float kBandwidthKhz = 250.0f;
constexpr uint8_t kSpreadingFactor = 11;
constexpr uint8_t kCodingRate = 5; // 4/5
constexpr uint8_t kSyncWord = 0x2B;
constexpr int8_t kTxPowerDbm = 22;
constexpr uint16_t kPreambleLen = 16;
constexpr uint8_t kHopLimit = 3;

// EU 869.4-869.65 MHz allows 10 % duty cycle.
constexpr uint32_t kAirtimeLimitMsPerHour = 360000;

// Contention window like Meshtastic: weaker links rebroadcast first.
constexpr float kSlotMs = 2.5f * ((1 << kSpreadingFactor) / kBandwidthKhz) + 7.2f;
constexpr uint8_t kCwMin = 3;
constexpr uint8_t kCwMax = 8;
constexpr float kSnrMin = -20;
constexpr float kSnrMax = 10;
constexpr uint8_t kMaxCadRetries = 8;

// Own message scheduling
constexpr uint32_t kCredentialsEveryMs = 10UL * 60 * 1000;
constexpr uint32_t kCredentialsMinGapMs = 30000;
constexpr uint32_t kMinPositionGapMs = 15000;
constexpr uint32_t kStationaryIntervalMs = 120000;
constexpr float kMoveTriggerM = 100;
constexpr float kTurnTriggerDeg = 30;
constexpr float kStationarySpeedMps = 1.0f;
constexpr uint32_t kHeardWindowMs = 30UL * 60 * 1000;
constexpr uint32_t kActiveWindowMs = 5UL * 60 * 1000;
constexpr uint32_t kHourMs = 60UL * 60 * 1000;
constexpr size_t kMaxHeard = 32;

struct OutMsg {
  uint32_t to;
  uint8_t len;
  uint8_t payload[kMaxUrlLen + 16];
};

struct PendingRelay {
  bool used = false;
  uint32_t from = 0;
  uint32_t id = 0;
  uint32_t dueMs = 0;
  uint8_t tries = 0;
  uint8_t len = 0;
  uint8_t buf[kMaxPacketLen];
};

struct SeenPacket {
  uint32_t from;
  uint32_t id;
};

struct HeardNode {
  uint32_t node = 0;
  uint32_t lastMs = 0;
  uint32_t lastPosMs = 0;
  uint32_t positions = 0;
  uint8_t hops = 0;
  float snr = 0;
  int16_t rssi = 0;
};

enum class TxResult { Sent, Busy, Blocked, Failed };

SPIClass loraSpi(HSPI);
SX1262 *radio = nullptr;
TaskHandle_t task = nullptr;
QueueHandle_t txQueue = nullptr;
QueueHandle_t rxQueue = nullptr;
portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;

uint8_t key[32];
size_t keyLen = 0;
uint8_t chanHash = 0;
uint32_t nodeNum = 0;

std::atomic<bool> wantActive{false};
std::atomic<bool> credRequested{false};
volatile bool dio1Flag = false;

MeshStats st;
HeardNode heard[kMaxHeard];
SeenPacket seen[64];
uint8_t seenNext = 0;
PendingRelay relays[4];

uint32_t airtimeBucketMs[60];
uint32_t airtimeBucketMinute[60];

void IRAM_ATTR onDio1() {
  dio1Flag = true;
  BaseType_t woken = pdFALSE;
  if (task) vTaskNotifyGiveFromISR(task, &woken);
  if (woken) portYIELD_FROM_ISR();
}

// ---- airtime ----

uint32_t airtimeLastHourMs() {
  uint32_t minute = millis() / 60000;
  uint32_t sum = 0;
  for (int i = 0; i < 60; i++) {
    if (minute - airtimeBucketMinute[i] < 60) sum += airtimeBucketMs[i];
  }
  return sum;
}

void addAirtime(uint32_t ms) {
  uint32_t minute = millis() / 60000;
  int i = minute % 60;
  if (airtimeBucketMinute[i] != minute) {
    airtimeBucketMinute[i] = minute;
    airtimeBucketMs[i] = 0;
  }
  airtimeBucketMs[i] += ms;
}

// ---- helpers ----

bool alreadySeen(uint32_t from, uint32_t id) {
  for (const SeenPacket &s : seen) {
    if (s.from == from && s.id == id) return true;
  }
  return false;
}

void markSeen(uint32_t from, uint32_t id) {
  seen[seenNext] = {from, id};
  seenNext = (seenNext + 1) % (sizeof(seen) / sizeof(seen[0]));
}

// Call with statsMux held.
void markHeard(uint32_t node, uint8_t type, uint8_t hops, float snr, int16_t rssi) {
  uint32_t now = millis();
  size_t slot = 0;
  bool found = false;
  for (size_t i = 0; i < kMaxHeard; i++) {
    if (heard[i].node == node) {
      slot = i;
      found = true;
      break;
    }
    if (heard[i].lastMs < heard[slot].lastMs) slot = i;
  }
  HeardNode &h = heard[slot];
  if (!found) {
    h = HeardNode();
    h.node = node;
  }
  h.lastMs = now;
  h.hops = hops;
  h.snr = snr;
  h.rssi = rssi;
  if (type == MsgPosition) {
    h.lastPosMs = now;
    h.positions++;
  }
}

void aesCtr(uint32_t packetId, uint32_t from, uint8_t *data, size_t len) {
  uint8_t nonce[16];
  uint8_t stream[16];
  size_t off = 0;
  makeNonce(packetId, from, nonce);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, key, keyLen * 8);
  mbedtls_aes_crypt_ctr(&ctx, len, &off, nonce, stream, data, data);
  mbedtls_aes_free(&ctx);
}

uint32_t cwDelayMs(uint8_t cw) { return (uint32_t)(random(0, 1 << cw) * kSlotMs); }

uint32_t relayDelayMs(float snr) {
  float t = (constrain(snr, kSnrMin, kSnrMax) - kSnrMin) / (kSnrMax - kSnrMin);
  uint8_t cw = kCwMin + (uint8_t)lroundf(t * (kCwMax - kCwMin));
  return (uint32_t)(2 * kCwMax * kSlotMs) + cwDelayMs(cw);
}

void startRx() {
  dio1Flag = false;
  radio->startReceive();
}

TxResult transmit(uint8_t *buf, size_t len) {
  uint32_t toa = radio->getTimeOnAir(len) / 1000;
  if (airtimeLastHourMs() + toa > kAirtimeLimitMsPerHour) return TxResult::Blocked;

  int16_t cad = radio->scanChannel();
  if (cad == RADIOLIB_LORA_DETECTED) {
    startRx();
    return TxResult::Busy;
  }

  int16_t res = radio->transmit(buf, len);
  addAirtime(toa);
  startRx();
  if (res != RADIOLIB_ERR_NONE) {
    Serial.printf("[mesh] transmit failed %d\n", res);
    return TxResult::Failed;
  }
  return TxResult::Sent;
}

void scheduleRelay(const uint8_t *buf, size_t len, const Header &h, float snr) {
  PendingRelay *slot = nullptr;
  for (PendingRelay &r : relays) {
    if (!r.used) {
      slot = &r;
      break;
    }
  }
  if (!slot) return;

  Header out = h;
  out.hopLimit = h.hopLimit - 1;
  out.relayNode = nodeNum & 0xFF;
  memcpy(slot->buf, buf, len);
  writeHeader(out, slot->buf);
  slot->len = len;
  slot->from = h.from;
  slot->id = h.id;
  slot->tries = 0;
  slot->dueMs = millis() + relayDelayMs(snr);
  slot->used = true;
}

void cancelRelay(uint32_t from, uint32_t id) {
  for (PendingRelay &r : relays) {
    if (r.used && r.from == from && r.id == id) r.used = false;
  }
}

void handleTrackerMessage(const Header &h, const uint8_t *payload, size_t len, float snr, int16_t rssi) {
  uint8_t type = messageType(payload, len);
  if (!type) return;
  uint8_t hops = h.hopStart >= h.hopLimit ? h.hopStart - h.hopLimit : 0;

  portENTER_CRITICAL(&statsMux);
  st.rxCount++;
  markHeard(h.from, type, hops, snr, rssi);
  portEXIT_CRITICAL(&statsMux);

  if (type == MsgCredRequest) {
    if (h.to == nodeNum) credRequested = true;
    return;
  }

  MeshRx rx;
  rx.from = h.from;
  rx.id = h.id;
  rx.type = type;
  rx.snr = snr;
  rx.hops = hops;
  if (type == MsgPosition) {
    if (!decodePosition(payload, len, rx.pos)) return;
  } else {
    std::string url;
    if (!decodeCredentials(payload, len, url, rx.urlHash)) return;
    strlcpy(rx.url, url.c_str(), sizeof(rx.url));
  }
  xQueueSend(rxQueue, &rx, 0);
}

void handleRx() {
  uint8_t buf[kMaxPacketLen + 1];
  size_t len = radio->getPacketLength();
  if (len > kMaxPacketLen) len = kMaxPacketLen;
  int16_t res = radio->readData(buf, len);
  if (res != RADIOLIB_ERR_NONE || len < kHeaderLen) return;

  float snr = radio->getSNR();
  int16_t rssi = (int16_t)radio->getRSSI();
  Header h = readHeader(buf);
  if (h.from == nodeNum || h.from == 0) return;

  if (alreadySeen(h.from, h.id)) {
    // Someone else relayed it already, no need for us to add another copy.
    cancelRelay(h.from, h.id);
    return;
  }
  markSeen(h.from, h.id);

  portENTER_CRITICAL(&statsMux);
  st.lastRxMs = millis();
  st.lastRssi = rssi;
  st.lastSnr = snr;
  portEXIT_CRITICAL(&statsMux);

  bool forUs = h.to == nodeNum;
  if (!forUs && h.hopLimit > 0 && (h.nextHop == 0 || h.nextHop == (nodeNum & 0xFF))) scheduleRelay(buf, len, h, snr);

  if (h.channel != chanHash || (h.to != kBroadcast && !forUs)) return;

  uint8_t plain[kMaxPacketLen];
  size_t plainLen = len - kHeaderLen;
  memcpy(plain, buf + kHeaderLen, plainLen);
  aesCtr(h.id, h.from, plain, plainLen);

  uint32_t port = 0;
  const uint8_t *payload;
  size_t payloadLen;
  if (!decodeData(plain, plainLen, port, payload, payloadLen) || port != kPortPrivateApp) return;
  handleTrackerMessage(h, payload, payloadLen, snr, rssi);
}

void processRelays() {
  uint32_t now = millis();
  for (PendingRelay &r : relays) {
    if (!r.used || (int32_t)(now - r.dueMs) < 0) continue;
    TxResult res = transmit(r.buf, r.len);
    if (res == TxResult::Busy && ++r.tries < kMaxCadRetries) {
      r.dueMs = millis() + cwDelayMs(kCwMax);
      continue;
    }
    r.used = false;
    if (res == TxResult::Sent) {
      portENTER_CRITICAL(&statsMux);
      st.relayCount++;
      portEXIT_CRITICAL(&statsMux);
    }
    return; // at most one transmission per pass so received packets are handled in between
  }
}

void processOwn() {
  static OutMsg msg;
  static bool havePending = false;
  static uint32_t notBeforeMs = 0;
  static uint8_t tries = 0;
  static uint8_t packet[kMaxPacketLen];
  static size_t packetLen = 0;

  if (!havePending) {
    if (xQueueReceive(txQueue, &msg, 0) != pdTRUE) return;
    Header h;
    h.to = msg.to;
    h.from = nodeNum;
    do {
      h.id = esp_random();
    } while (h.id == 0);
    h.hopLimit = kHopLimit;
    h.hopStart = kHopLimit;
    h.channel = chanHash;
    h.relayNode = nodeNum & 0xFF;
    writeHeader(h, packet);
    size_t dataLen = encodeData(kPortPrivateApp, msg.payload, msg.len, packet + kHeaderLen, sizeof(packet) - kHeaderLen);
    if (!dataLen) return;
    aesCtr(h.id, nodeNum, packet + kHeaderLen, dataLen);
    packetLen = kHeaderLen + dataLen;
    markSeen(nodeNum, h.id);
    havePending = true;
    tries = 0;
    notBeforeMs = millis() + cwDelayMs(kCwMin);
  }
  if ((int32_t)(millis() - notBeforeMs) < 0) return;

  TxResult res = transmit(packet, packetLen);
  if (res == TxResult::Busy && ++tries < kMaxCadRetries) {
    notBeforeMs = millis() + cwDelayMs(kCwMax);
    return;
  }
  havePending = false;

  portENTER_CRITICAL(&statsMux);
  if (res == TxResult::Sent) {
    st.txCount++;
    if (msg.payload[0] == MsgPosition) st.lastTxMs = millis();
  } else if (res == TxResult::Blocked) {
    st.txBlocked++;
  }
  portEXIT_CRITICAL(&statsMux);
}

void meshTask(void *) {
  bool active = false;
  for (;;) {
    bool want = wantActive;
    if (want != active) {
      active = want;
      if (active) {
        startRx();
      } else {
        radio->sleep();
        for (PendingRelay &r : relays) r.used = false;
        xQueueReset(txQueue);
      }
      portENTER_CRITICAL(&statsMux);
      st.active = active;
      portEXIT_CRITICAL(&statsMux);
      Serial.printf("[mesh] radio %s\n", active ? "receiving" : "asleep");
    }
    if (!active) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    if (dio1Flag) {
      dio1Flag = false;
      handleRx();
    }
    processRelays();
    processOwn();

    portENTER_CRITICAL(&statsMux);
    st.airtimePercent = airtimeLastHourMs() * 100.0f / 3600000.0f;
    portEXIT_CRITICAL(&statsMux);
  }
}

void enqueue(uint32_t to, const uint8_t *payload, size_t len) {
  if (!txQueue || !len || len > sizeof(OutMsg::payload)) return;
  OutMsg m;
  m.to = to;
  m.len = len;
  memcpy(m.payload, payload, len);
  xQueueSend(txQueue, &m, 0);
}

float distanceM(double lat1, double lon1, double lat2, double lon2) {
  const double r = 6371000.0;
  double x = (lon2 - lon1) * DEG_TO_RAD * cos((lat1 + lat2) * 0.5 * DEG_TO_RAD);
  double y = (lat2 - lat1) * DEG_TO_RAD;
  return (float)(sqrt(x * x + y * y) * r);
}

float headingDiff(float a, float b) {
  float d = fabsf(fmodf(a - b + 540.0f, 360.0f) - 180.0f);
  return d;
}

} // namespace

void begin() {
  if (radio) return;

  uint64_t mac = ESP.getEfuseMac();
  for (int i = 2; i < 6; i++) nodeNum = (nodeNum << 8) | ((mac >> (8 * i)) & 0xFF);
  st.nodeNum = nodeNum;
  st.placeholderKey = strcmp(MESH_PSK_B64, kPlaceholderKey) == 0;

  size_t olen = 0;
  int kr = mbedtls_base64_decode(key, sizeof(key), &olen, (const unsigned char *)MESH_PSK_B64, strlen(MESH_PSK_B64));
  if (kr != 0 || (olen != 16 && olen != 32)) {
    st.error = "MESH_PSK_B64 ungueltig";
    Serial.println("[mesh] MESH_PSK_B64 must be a base64 encoded 16 or 32 byte key");
    return;
  }
  keyLen = olen;
  chanHash = channelHash(MESH_CHANNEL_NAME, key, keyLen);
  if (st.placeholderKey) Serial.println("[mesh] WARNING: default mesh key in use, set MESH_PSK_B64 in platformio.ini");

  loraSpi.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
  radio = new SX1262(new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSpi));
  int16_t res = radio->begin(kFreqMhz, kBandwidthKhz, kSpreadingFactor, kCodingRate, kSyncWord, kTxPowerDbm,
                             kPreambleLen, LORA_TCXO_VOLTAGE, false);
  if (res != RADIOLIB_ERR_NONE) {
    st.error = "LoRa-Funk nicht gefunden";
    Serial.printf("[mesh] radio init failed %d\n", res);
    delete radio;
    radio = nullptr;
    return;
  }
  radio->setDio2AsRfSwitch(true);
  radio->setCurrentLimit(140);
  radio->setRxBoostedGainMode(true);
  radio->setPacketReceivedAction(onDio1);
  radio->sleep();

  txQueue = xQueueCreate(4, sizeof(OutMsg));
  rxQueue = xQueueCreate(8, sizeof(MeshRx));
  st.ok = true;
  xTaskCreatePinnedToCore(meshTask, "mesh", 8192, nullptr, 3, &task, 0);
  Serial.printf("[mesh] node !%08lx, channel %s (hash 0x%02x), %.3f MHz SF%u BW%.0f\n", (unsigned long)nodeNum,
                MESH_CHANNEL_NAME, chanHash, kFreqMhz, kSpreadingFactor, kBandwidthKhz);
}

void setActive(bool on) { wantActive = on && radio; }

void loop(const GnssFix &fix, const String &url, uint16_t intervalSec, bool sendOwn) {
  static uint32_t lastCredMs = 0;
  static bool credSent = false;
  static uint32_t lastPosMs = 0;
  static bool posSent = false;
  static double lastLat = 0, lastLon = 0;
  static float lastHeading = 0;

  if (!radio || !wantActive || !url.length()) return;
  uint32_t now = millis();
  std::string u(url.c_str());
  uint8_t buf[kMaxUrlLen + 16];

  bool credDue = sendOwn && (!credSent || now - lastCredMs >= kCredentialsEveryMs);
  if (credRequested.exchange(false) && (!credSent || now - lastCredMs >= kCredentialsMinGapMs)) credDue = true;
  if (credDue) {
    size_t n = encodeCredentials(u, buf, sizeof(buf));
    if (n) enqueue(kBroadcast, buf, n);
    lastCredMs = now;
    credSent = true;
  }

  if (!sendOwn) {
    posSent = false;
    return;
  }
  if (!fix.valid) return;

  uint32_t since = now - lastPosMs;
  uint32_t interval = (uint32_t)intervalSec * 1000;
  float moved = distanceM(lastLat, lastLon, fix.latitude, fix.longitude);
  bool stationary = fix.speedMps < kStationarySpeedMps && moved < kMoveTriggerM / 4;
  bool due;
  if (!posSent) due = true;
  else if (stationary) due = since >= max(interval, kStationaryIntervalMs);
  else if (since >= interval) due = true;
  else due = since >= kMinPositionGapMs &&
             (moved >= kMoveTriggerM || headingDiff(fix.heading, lastHeading) >= kTurnTriggerDeg);
  if (!due) return;

  Position p;
  p.latitude = fix.latitude;
  p.longitude = fix.longitude;
  p.altitude = fix.altitude;
  p.speedMps = fix.speedMps;
  p.heading = fix.heading;
  p.accuracy = fix.accuracy;
  p.fixTime = fix.unixTime;
  p.urlHash = urlHash(u);
  size_t n = encodePosition(p, buf, sizeof(buf));
  if (n) enqueue(kBroadcast, buf, n);

  lastPosMs = now;
  posSent = true;
  lastLat = fix.latitude;
  lastLon = fix.longitude;
  lastHeading = fix.heading;
}

bool receive(MeshRx &out) { return rxQueue && xQueueReceive(rxQueue, &out, 0) == pdTRUE; }

void requestCredentials(uint32_t node) {
  uint8_t buf[4];
  size_t n = encodeCredRequest(buf, sizeof(buf));
  enqueue(node, buf, n);
}

MeshStats stats() {
  portENTER_CRITICAL(&statsMux);
  MeshStats s = st;
  uint32_t now = millis();
  s.heardNodes = s.activeNodes = s.directNodes = s.positionNodes1h = 0;
  for (const HeardNode &h : heard) {
    if (!h.node) continue;
    uint32_t age = now - h.lastMs;
    if (age < kHeardWindowMs) s.heardNodes++;
    if (age < kActiveWindowMs) {
      s.activeNodes++;
      if (h.hops == 0) s.directNodes++;
    }
    if (h.positions && now - h.lastPosMs < kHourMs) s.positionNodes1h++;
  }
  portEXIT_CRITICAL(&statsMux);
  return s;
}

size_t nodes(MeshNodeInfo *out, size_t max) {
  HeardNode copy[kMaxHeard];
  portENTER_CRITICAL(&statsMux);
  memcpy(copy, heard, sizeof(copy));
  portEXIT_CRITICAL(&statsMux);

  uint32_t now = millis();
  size_t n = 0;
  for (const HeardNode &h : copy) {
    if (!h.node || now - h.lastMs >= kHourMs) continue;
    MeshNodeInfo info;
    info.node = h.node;
    info.lastAgoSec = (now - h.lastMs) / 1000;
    info.lastPosAgoSec = h.positions ? (int32_t)((now - h.lastPosMs) / 1000) : -1;
    info.positions = h.positions;
    info.hops = h.hops;
    info.snr = h.snr;
    info.rssi = h.rssi;

    size_t pos = n < max ? n : max;
    while (pos > 0 && out[pos - 1].lastAgoSec > info.lastAgoSec) {
      if (pos < max) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < max) out[pos] = info;
    if (n < max) n++;
  }
  return n;
}

} // namespace mesh
