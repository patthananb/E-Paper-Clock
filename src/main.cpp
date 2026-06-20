// ClaudeMeterEpaper — Claude Code usage on Waveshare ESP32-S3-ePaper-1.54.
//
// Acts as the BLE peripheral the Clawdmeter daemon expects. The daemon
// (https://github.com/HermannBjorgvin/Clawdmeter, daemon/ dir) polls the
// Anthropic rate-limit headers and writes this JSON to the RX characteristic:
//
//   {"s":<5h util %>,"sr":<5h reset min>,"w":<7d util %>,"wr":<7d reset min>,
//    "st":"<status>","ok":true}
//
// We parse it and draw a usage meter on the 200x200 SSD1681 panel.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include "wifi_config.h"

// ---- EPD pins (Waveshare ESP32-S3-ePaper-1.54) ----
#define EPD_SCK  12
#define EPD_MOSI 13
#define EPD_CS   11
#define EPD_DC   10
#define EPD_RST   9
#define EPD_BUSY  8
#define EPD_PWR   6   // active-low power gate: LOW = panel ON

// ---- Buttons (pins from Waveshare ESP32-S3-ePaper-1.54 demo) ----
#define BOOT_BTN_PIN  0      // BOOT: press = blink heartbeat (active-low, pull-up)
#define PWR_BTN_PIN   18     // PWR : click = cycle screen; hold 3s = latch power
#define VBAT_PWR_PIN  17     // drive HIGH to latch battery power on

// ---- I2C (SHTC3 temp/humidity + PCF85063 RTC share the bus) ----
#define I2C_SDA_PIN   47
#define I2C_SCL_PIN   48
#define SHTC3_ADDR    0x70

// ---- Battery sense (same board as BatteryCheck) ----
#define BAT_ADC_PIN   4      // ADC1_CH3
#define BAT_DIVIDER   2.0f   // on-board /2 resistor divider
#define BAT_SAMPLES   16

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ---- BLE GATT contract (must match the daemon) ----
static const char* DEVICE_NAME  = "Clawdmeter";
static const char* SERVICE_UUID = "4c41555a-4465-7669-6365-000000000001";
static const char* RX_CHAR_UUID = "4c41555a-4465-7669-6365-000000000002"; // daemon -> us
static const char* REQ_CHAR_UUID= "4c41555a-4465-7669-6365-000000000004"; // us -> daemon (notify)

// ---- Shared state between BLE callbacks and loop() ----
static volatile bool   g_haveNew   = false;   // new payload to render
static volatile bool   g_connEvent = false;   // connect/disconnect changed
static volatile bool   g_connected = false;
static String          g_payload;             // latest raw JSON
static portMUX_TYPE    g_mux = portMUX_INITIALIZER_UNLOCKED;

// ---- Advertising ----
// Keep advertising until a daemon connects. 'p' force-restarts advertising.
static volatile bool   g_advertising = false;  // advertising and waiting for connect
static uint32_t        g_lastAdvLog  = 0;       // last 1s "searching" heartbeat

static void startAdv();                         // fwd decl (used in onDisconnect)

// ---- Top-right status (alive blink + battery gauge), refreshed every 15s ----
static uint32_t        g_lastTopRight = 0;      // last partial refresh (millis)
static bool            g_aliveDot     = false;  // toggles each refresh = "alive"
static int             g_batPct       = 0;      // last battery percent

// ---- Screen mode (PWR click cycles) ----
enum Screen { SCREEN_USAGE, SCREEN_CLOCK, SCREEN_COUNT };
static Screen          g_screen     = SCREEN_USAGE;
static float           g_tempC      = NAN;      // last SHTC3 reading

// Parsed metrics
struct Metrics {
  int  s = -1, sr = 0, w = -1, wr = 0;
  char st[16] = "";
  bool ok = false;
};
static Metrics g_m;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    portENTER_CRITICAL(&g_mux);
    g_payload = String(c->getValue().c_str());
    g_haveNew = true;
    portEXIT_CRITICAL(&g_mux);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    g_connected = true;  g_connEvent = true;
    g_advertising = false;   // connected: stop searching heartbeat
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    g_connected = false; g_connEvent = true;
    startAdv();              // resume advertising until reconnect
  }
};

// ---- Battery ----
// Single-cell Li-po discharge curve (open-circuit-ish). Volts -> percent.
static int batteryPercent(float v) {
  static const float pts[][2] = {
    {4.20f,100},{4.10f,90},{4.00f,80},{3.90f,70},{3.80f,60},
    {3.70f,50},{3.60f,35},{3.50f,20},{3.40f,12},{3.30f,6},
    {3.20f,3},{3.00f,0}};
  const int n = sizeof(pts) / sizeof(pts[0]);
  if (v >= pts[0][0])   return 100;
  if (v <= pts[n-1][0]) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (v <= pts[i][0] && v > pts[i+1][0]) {
      float t = (v - pts[i+1][0]) / (pts[i][0] - pts[i+1][0]);
      return (int)roundf(pts[i+1][1] + t * (pts[i][1] - pts[i+1][1]));
    }
  }
  return 0;
}

static float readBatteryVolts() {
  uint32_t mv = 0;
  for (int i = 0; i < BAT_SAMPLES; i++) mv += analogReadMilliVolts(BAT_ADC_PIN);
  mv /= BAT_SAMPLES;
  return (mv / 1000.0f) * BAT_DIVIDER;
}

// ---- SHTC3 temperature (I2C). Returns °C, or leaves out untouched on error. ----
static bool readSHTC3(float& tC) {
  Wire.beginTransmission(SHTC3_ADDR);          // wake-up
  Wire.write(0x35); Wire.write(0x17);
  if (Wire.endTransmission() != 0) return false;
  delayMicroseconds(300);

  Wire.beginTransmission(SHTC3_ADDR);          // measure T first, no clock-stretch
  Wire.write(0x78); Wire.write(0x66);
  if (Wire.endTransmission() != 0) return false;
  delay(15);                                   // ~12ms conversion

  if (Wire.requestFrom(SHTC3_ADDR, 6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];
  tC = -45.0f + 175.0f * (float)rawT / 65535.0f;

  Wire.beginTransmission(SHTC3_ADDR);          // sleep
  Wire.write(0xB0); Wire.write(0x98);
  Wire.endTransmission();
  return true;
}

// ---- WiFi NTP one-shot: connect, sync system clock, then drop WiFi (so it
// doesn't fight NimBLE for the radio). System time persists while powered. ----
static void wifiTimeSync() {
  if (strlen(WIFI_SSID) == 0) { Serial.println("wifi: no SSID set, clock disabled"); return; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("wifi: connecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(250); Serial.print("."); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("wifi: failed"); WiFi.mode(WIFI_OFF); return; }

  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  struct tm tm;
  if (getLocalTime(&tm, 8000))
    Serial.printf("ntp: %04d-%02d-%02d %02d:%02d\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  else
    Serial.println("ntp: no time");
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
}

// ---- Rendering ----
// Reset countdown as "Xh Ym" (or "Ym" under an hour).
static void fmtHM(int mins, char* out, size_t n) {
  if (mins < 0) mins = 0;
  int h = mins / 60, m = mins % 60;
  if (h > 0) snprintf(out, n, "%dh %dm", h, m);
  else       snprintf(out, n, "%dm", m);
}

// Longer countdown: "Xd Yh" while >= 24h, then falls back to "Xh Ym".
static void fmtDHM(int mins, char* out, size_t n) {
  if (mins < 0) mins = 0;
  if (mins >= 24 * 60) {
    int d = mins / (24 * 60), h = (mins % (24 * 60)) / 60;
    snprintf(out, n, "%dd %dh", d, h);
  } else {
    fmtHM(mins, out, n);
  }
}

// Top-right corner: a 4-segment battery icon (25/50/75/100%) left of a small
// alive dot. Drawn by the full renders and the 15s partial refresh alike.
static void drawTopRight(int batPct, bool dot) {
  // Alive dot, far top-right corner.
  const int cx = 190, cy = 11, r = 6;
  if (dot) display.fillCircle(cx, cy, r, GxEPD_BLACK);
  else     display.drawCircle(cx, cy, r, GxEPD_BLACK);

  // Battery body + terminal nub, left of the dot with a little gap.
  const int bx = 146, by = 6, bw = 28, bh = 10;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  display.fillRect(bx + bw, by + 3, 2, bh - 6, GxEPD_BLACK);

  // Fill N of 4 segments. <13%->0, then 25/50/75/100 thresholds.
  int segs = (batPct + 12) / 25;
  if (segs < 0) segs = 0;
  if (segs > 4) segs = 4;
  const int segW = 5, gap = 1, ix = bx + 1, iy = by + 1, ih = bh - 2;
  for (int i = 0; i < segs; i++)
    display.fillRect(ix + i * (segW + gap), iy, segW, ih, GxEPD_BLACK);
}

static void drawBar(int x, int y, int w, int h, int pct) {
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  int fill = (w - 2) * pct / 100;
  if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, GxEPD_BLACK);
}

static void renderMetrics(const Metrics& m) {
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Header
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 24);
    display.print("Claude");
    drawTopRight(g_batPct, g_aliveDot);
    display.drawFastHLine(0, 34, 200, GxEPD_BLACK);

    char rbuf[12];

    // 5h block
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 64);
    display.print("5h");
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(70, 68);
    display.printf("%d%%", m.s < 0 ? 0 : m.s);
    drawBar(8, 78, 184, 14, m.s);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 110);
    fmtHM(m.sr, rbuf, sizeof(rbuf));
    display.printf("reset %s", rbuf);

    display.drawFastHLine(0, 122, 200, GxEPD_BLACK);

    // 7d block
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 150);
    display.print("7d");
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(70, 154);
    display.printf("%d%%", m.w < 0 ? 0 : m.w);
    drawBar(8, 164, 184, 14, m.w);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 196);
    fmtDHM(m.wr, rbuf, sizeof(rbuf));
    display.printf("reset %s", rbuf);
  } while (display.nextPage());
}

static void renderStatus(const char* line) {
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(8, 24);
    display.print("Claude");
    drawTopRight(g_batPct, g_aliveDot);
    display.drawFastHLine(0, 34, 200, GxEPD_BLACK);
    display.setCursor(8, 110);
    display.print(line);
  } while (display.nextPage());
}

// Partial refresh of just the top-right box (battery + alive dot). Fast, no
// full-screen flash — safe to call every 15s.
static void updateTopRight() {
  display.setRotation(0);
  display.setPartialWindow(104, 0, 96, 33);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawTopRight(g_batPct, g_aliveDot);
  } while (display.nextPage());
}

// Clock screen: big HH:MM, date, and SHTC3 temperature.
static void renderClock() {
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 24);
    display.print("Clock");
    drawTopRight(g_batPct, g_aliveDot);
    display.drawFastHLine(0, 34, 200, GxEPD_BLACK);

    struct tm tm;
    bool haveTime = getLocalTime(&tm, 50);
    int16_t bx, by; uint16_t bw, bh;

    char t[16];
    if (haveTime) strftime(t, sizeof(t), "%H:%M", &tm);
    else          strcpy(t, "--:--");
    display.setFont(&FreeMonoBold24pt7b);
    display.getTextBounds(t, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((200 - bw) / 2 - bx, 100);
    display.print(t);

    if (haveTime) {
      char d[24];
      strftime(d, sizeof(d), "%a %d %b", &tm);
      display.setFont(&FreeMonoBold12pt7b);
      display.getTextBounds(d, 0, 0, &bx, &by, &bw, &bh);
      display.setCursor((200 - bw) / 2 - bx, 135);
      display.print(d);
    }

    char c[16];
    if (!isnan(g_tempC)) snprintf(c, sizeof(c), "%.1f C", g_tempC);
    else                 strcpy(c, "-- C");
    display.setFont(&FreeMonoBold18pt7b);
    display.getTextBounds(c, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((200 - bw) / 2 - bx, 185);
    display.print(c);
  } while (display.nextPage());
}

// Render whichever screen is active (refresh temp before drawing the clock).
static void renderCurrent() {
  if (g_screen == SCREEN_CLOCK) {
    readSHTC3(g_tempC);
    renderClock();
  } else {
    renderMetrics(g_m);
  }
}

static bool parsePayload(const String& json, Metrics& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  out.s  = doc["s"]  | -1;
  out.sr = doc["sr"] | 0;
  out.w  = doc["w"]  | -1;
  out.wr = doc["wr"] | 0;
  out.ok = doc["ok"] | false;
  const char* st = doc["st"] | "";
  strncpy(out.st, st, sizeof(out.st) - 1);
  out.st[sizeof(out.st) - 1] = 0;
  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(PWR_BTN_PIN,  INPUT_PULLUP);
  pinMode(VBAT_PWR_PIN, OUTPUT);
  digitalWrite(VBAT_PWR_PIN, HIGH);                // latch battery power on

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  analogReadResolution(12);
  analogRead(BAT_ADC_PIN);                         // configure pin as analog first
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);  // full ~0-3.3V range

  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, LOW);          // panel ON
  delay(10);

  SPI.end();
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);

  g_batPct = batteryPercent(readBatteryVolts());  // so the boot screen shows it
  readSHTC3(g_tempC);
  renderStatus("waiting BLE...");

  wifiTimeSync();   // one-shot NTP before BLE radio comes up

  // BLE peripheral matching the Clawdmeter daemon's expectations.
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic* rx = svc->createCharacteristic(
      RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  svc->createCharacteristic(REQ_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();

  // The 128-bit service UUID (18B) + flags (3B) nearly fill the 31B primary
  // advert, leaving no room for the name. Put the UUID in the primary packet
  // and the name in the scan response, so macOS (matches by name) can see it.
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));

  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->enableScanResponse(true);

  startAdv();                      // advertise until a daemon connects
  Serial.println("ready (press 'p' to restart advertising)");
}

// Start advertising and keep it up until a connection arrives.
static void startAdv() {
  NimBLEDevice::startAdvertising();
  g_advertising = true;
  g_lastAdvLog  = 0;               // force immediate heartbeat
  Serial.printf("advertising as %s\n", DEVICE_NAME);
}

void loop() {
  // BOOT button: each press blinks the heartbeat dot immediately.
  static int      lastBtn   = HIGH;
  static uint32_t lastBtnMs = 0;
  int btn = digitalRead(BOOT_BTN_PIN);
  if (btn != lastBtn && millis() - lastBtnMs > 40) {   // debounced edge
    lastBtnMs = millis();
    lastBtn   = btn;
    if (btn == LOW) {                                  // pressed
      g_aliveDot     = !g_aliveDot;
      g_lastTopRight = millis();                       // resync auto-blink
      updateTopRight();
    }
  }

  // PWR button: each click cycles the screen (usage <-> clock).
  static int      lastPwr   = HIGH;
  static uint32_t lastPwrMs = 0;
  int pwr = digitalRead(PWR_BTN_PIN);
  if (pwr != lastPwr && millis() - lastPwrMs > 50) {
    lastPwrMs = millis();
    lastPwr   = pwr;
    Serial.printf("PWR pin -> %d\n", pwr);             // calibrate polarity
    if (pwr == LOW) {                                  // pressed (assume active-low)
      g_screen = (Screen)((g_screen + 1) % SCREEN_COUNT);
      Serial.printf("screen -> %s\n", g_screen == SCREEN_CLOCK ? "clock" : "usage");
      renderCurrent();
    }
  }

  // Every 15s: blink the alive dot + refresh battery, via a fast partial
  // update of the top-right box only (no full-screen flash).
  if (millis() - g_lastTopRight >= 15000) {
    g_lastTopRight = millis();
    g_aliveDot = !g_aliveDot;
    g_batPct   = batteryPercent(readBatteryVolts());
    updateTopRight();
  }

  // Serial command: 'p' force-restarts advertising.
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == 'p' || ch == 'P') startAdv();
  }

  // While advertising and not yet connected: heartbeat every 1s.
  if (g_advertising && !g_connected) {
    uint32_t now = millis();
    if (now - g_lastAdvLog >= 1000) {
      g_lastAdvLog = now;
      Serial.println("searching for daemon...");
    }
  }

  if (g_connEvent) {
    g_connEvent = false;
    if (!g_connected) { if (g_screen == SCREEN_USAGE) renderStatus("disconnected"); }
    else              Serial.println("connected");
  }

  if (g_haveNew) {
    String json;
    portENTER_CRITICAL(&g_mux);
    json = g_payload;
    g_haveNew = false;
    portEXIT_CRITICAL(&g_mux);

    Serial.printf("payload: %s\n", json.c_str());
    if (parsePayload(json, g_m)) {
      if (g_screen == SCREEN_USAGE) renderMetrics(g_m);  // don't clobber clock view
    } else {
      Serial.println("bad JSON");
    }
  }

  delay(20);
}
