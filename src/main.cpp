// ClaudeMeterEpaper — Claude usage + clock on a Waveshare ESP32-S3-ePaper-1.54.
// BLE peripheral for the Clawdmeter daemon.
//
// Modes (PWR button cycles): Claude usage <-> Clock.

#include <Arduino.h>
#include <Wire.h>

#include "Pins.h"
#include "UsageData.h"
#include "BatteryGauge.h"
#include "TempSensor.h"
#include "TimeSync.h"
#include "Buttons.h"
#include "ClawdBle.h"
#include "DisplayUi.h"

using namespace cm;

static DisplayUi    ui;
static BatteryGauge battery;
static TempSensor   temp;
static TimeSync     timeSync;
static Buttons      buttons;
static ClawdBle     ble;
static UsageData    usage;

enum Mode { MODE_USAGE, MODE_CLOCK, MODE_COUNT };
static Mode     g_mode          = MODE_USAGE;
static uint32_t g_lastBatteryMs = 0;   // charging sweep / battery refresh (10s)
static uint32_t g_lastClockDraw = 0;   // clock minute tick (partial, 1min)
static uint32_t g_lastClockFull = 0;   // clock full refresh (ghost clean, 30min)
static bool     g_clockShowHum  = false; // clock bottom line: false=temp, true=humidity
static uint32_t g_lastUsageDraw = 0;   // usage re-check (10min)
static uint32_t g_lastPayloadMs = 0;   // last daemon payload arrival

static constexpr uint32_t DAEMON_ALIVE_MS = 130000;  // ~2 daemon poll periods

static bool daemonAlive() {
  return ble.isConnected() && (millis() - g_lastPayloadMs < DAEMON_ALIVE_MS);
}

static void renderCurrent() {
  int b = battery.lastPercent();
  switch (g_mode) {
    case MODE_USAGE:
      ui.showUsage(usage, b, daemonAlive());
      break;
    case MODE_CLOCK: {
      temp.read();
      struct tm tm;
      bool haveTime = timeSync.localTime(tm);
      ui.showClock(haveTime, tm, g_clockShowHum, temp.lastC(), temp.lastRH(), b);
      break;
    }
    default: break;
  }
}

void setup() {
  Serial.begin(115200);

  buttons.begin();
  pinMode(Pins::VBAT_PWR, OUTPUT);
  digitalWrite(Pins::VBAT_PWR, HIGH);              // latch battery power on

  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  battery.begin();
  temp.read();

  ui.begin();
  ui.showStatus("waiting BLE...", battery.lastPercent());

  timeSync.begin();   // one-shot NTP before BLE radio comes up
  ble.begin();

  Serial.println("ready (press 'p' to restart advertising)");
}

void loop() {
  buttons.poll();

  // PWR click: cycle mode. Re-arm the per-mode refresh timers from now.
  if (buttons.tookPwrClick()) {
    g_mode = (Mode)((g_mode + 1) % MODE_COUNT);
    g_lastClockDraw = g_lastUsageDraw = g_lastClockFull = millis();
    Serial.printf("mode -> %d\n", g_mode);
    renderCurrent();   // full draw lays down the clock baseline for partials
  }

  // BOOT short click: action depends on current mode.
  if (buttons.tookBootClick()) {
    if (g_mode == MODE_USAGE) {
      // Ask the daemon for fresh data and re-render immediately.
      Serial.println("boot click: requesting daemon refresh");
      ble.requestRefresh();
      g_lastUsageDraw = millis();            // re-arm 10-min auto cycle
      renderCurrent();
    } else if (g_mode == MODE_CLOCK) {
      // Toggle temp/humidity and refresh the time (partial, no flash).
      g_clockShowHum = !g_clockShowHum;
      g_lastClockDraw = millis();
      temp.read();
      struct tm tm;
      bool haveTime = timeSync.localTime(tm);
      ui.updateClockTime(haveTime, tm);
      ui.updateClockReading(g_clockShowHum, temp.lastC(), temp.lastRH());
    }
  }

  // BOOT long hold in clock mode: retry the WiFi/NTP sync, then redraw the clock
  // (~12s blocking while WiFi connects; BLE drops out briefly, then resumes).
  if (buttons.tookBootHold() && g_mode == MODE_CLOCK) {
    Serial.println("boot hold: retrying wifi");
    timeSync.retry();
    g_lastClockDraw = g_lastClockFull = millis();
    temp.read();
    struct tm tm;
    bool haveTime = timeSync.localTime(tm);
    ui.showClock(haveTime, tm, g_clockShowHum, temp.lastC(), temp.lastRH(),
                 battery.lastPercent(), /*forceFull=*/true);
  }

  // Clock: every minute redraw the HH:MM line and cycle the bottom reading
  // between temperature and humidity (partial, no flash). Once every 30 min do
  // a full refresh to clear e-paper ghosting.
  if (g_mode == MODE_CLOCK && millis() - g_lastClockDraw >= 60000) {
    g_lastClockDraw = millis();
    g_clockShowHum = !g_clockShowHum;      // cycle temp <-> humidity each minute
    temp.read();
    if (millis() - g_lastClockFull >= 1800000) {
      g_lastClockFull = millis();
      struct tm tm;
      bool haveTime = timeSync.localTime(tm);
      ui.showClock(haveTime, tm, g_clockShowHum, temp.lastC(), temp.lastRH(),
                   battery.lastPercent(), /*forceFull=*/true);  // ghost clean, one flash / 30 min
    } else {
      struct tm tm;
      bool haveTime = timeSync.localTime(tm);
      ui.updateClockTime(haveTime, tm);                                  // number only, no flash
      ui.updateClockReading(g_clockShowHum, temp.lastC(), temp.lastRH()); // temp/humidity, no flash
    }
  }

  // Usage: re-render every 10 min so the daemon-alive dot stays current.
  if (g_mode == MODE_USAGE && millis() - g_lastUsageDraw >= 600000) {
    g_lastUsageDraw = millis();
    renderCurrent();
  }

  // Serial 'p' -> restart advertising.
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'p' || c == 'P') ble.restartAdvertising();
  }

  // While advertising and not connected: 1s "searching" heartbeat (serial).
  static uint32_t lastAdvLog = 0;
  if (ble.isAdvertising() && !ble.isConnected() && millis() - lastAdvLog >= 1000) {
    lastAdvLog = millis();
    Serial.println("searching for daemon...");
  }

  // BLE connect/disconnect.
  bool connected;
  if (ble.takeConnEvent(connected)) {
    if (connected) Serial.println("connected");
    else if (g_mode == MODE_USAGE) ui.showStatus("disconnected", battery.lastPercent());
  }

  // New usage payload.
  String json;
  if (ble.takePayload(json)) {
    Serial.printf("payload: %s\n", json.c_str());
    if (parseUsage(json, usage)) {
      g_lastPayloadMs = millis();
      if (g_mode == MODE_USAGE) ui.showUsage(usage, battery.lastPercent(), daemonAlive());
    } else {
      Serial.println("bad JSON");
    }
  }

  // Every 10s: refresh battery. While charging, play the left-to-right sweep.
  if (millis() - g_lastBatteryMs >= 10000) {
    g_lastBatteryMs = millis();
    battery.readPercent();
    if (battery.isCharging()) ui.sweepCharging(battery.lastPercent());
    else                      ui.refreshBattery(battery.lastPercent());
  }

  delay(20);
}
