// ClaudeMeterEpaper — Claude usage / clock / pomodoro on a Waveshare
// ESP32-S3-ePaper-1.54. BLE peripheral for the Clawdmeter daemon.
//
// Modes (PWR button cycles): Claude usage -> Clock -> Pomodoro.
// Pomodoro: hold BOOT to start a 25-min timer, short-press BOOT to stop.
// Beep on finish is stubbed pending ES8311 driver integration (see Beeper).

#include <Arduino.h>
#include <Wire.h>

#include "Pins.h"
#include "UsageData.h"
#include "BatteryGauge.h"
#include "TempSensor.h"
#include "TimeSync.h"
#include "Buttons.h"
#include "Pomodoro.h"
#include "Beeper.h"
#include "ClawdBle.h"
#include "DisplayUi.h"

using namespace cm;

static DisplayUi    ui;
static BatteryGauge battery;
static TempSensor   temp;
static TimeSync     timeSync;
static Buttons      buttons;
static Pomodoro     pomo;
static Beeper       beeper;
static ClawdBle     ble;
static UsageData    usage;

enum Mode { MODE_USAGE, MODE_CLOCK, MODE_POMODORO, MODE_COUNT };
static Mode     g_mode          = MODE_USAGE;
static bool     g_pomoTimeUp    = false;
static uint32_t g_lastBatteryMs = 0;   // charging sweep / battery refresh (10s)
static uint32_t g_lastPomoDraw  = 0;   // pomodoro tick (5s)
static uint32_t g_lastClockDraw = 0;   // clock refresh (1min)
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
      ui.showClock(haveTime, tm, temp.lastC(), b);
      break;
    }
    case MODE_POMODORO:
      ui.showPomodoro(pomo.remainingMin(), pomo.remainingSec(),
                      pomo.fraction(), pomo.isActive(), g_pomoTimeUp, b);
      break;
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
  beeper.begin();
  ble.begin();

  Serial.println("ready (press 'p' to restart advertising)");
}

void loop() {
  buttons.poll();

  // PWR click: cycle mode. Re-arm the per-mode refresh timers from now.
  if (buttons.tookPwrClick()) {
    g_mode = (Mode)((g_mode + 1) % MODE_COUNT);
    g_pomoTimeUp = false;
    g_lastClockDraw = g_lastUsageDraw = g_lastPomoDraw = millis();
    Serial.printf("mode -> %d\n", g_mode);
    renderCurrent();
  }

  // BOOT hold: start pomodoro and jump to its screen.
  if (buttons.tookBootHold()) {
    pomo.start();
    g_pomoTimeUp   = false;
    g_mode         = MODE_POMODORO;
    g_lastPomoDraw = millis();
    renderCurrent();
  }

  // BOOT short press: stop a running pomodoro.
  if (buttons.tookBootShort() && pomo.isActive()) {
    pomo.stop();
    renderCurrent();
  }

  // Pomodoro finished -> beep + show TIME UP.
  if (pomo.tickFinished()) {
    g_pomoTimeUp = true;
    beeper.beep();
    if (g_mode == MODE_POMODORO) renderCurrent();
  }

  // Pomodoro countdown: fast partial redraw of the ring + time every 5s.
  if (g_mode == MODE_POMODORO && pomo.isActive() &&
      millis() - g_lastPomoDraw >= 5000) {
    g_lastPomoDraw = millis();
    ui.updatePomodoro(pomo.remainingMin(), pomo.remainingSec(),
                      pomo.fraction(), true, false);
  }

  // Clock: full refresh once a minute while shown.
  if (g_mode == MODE_CLOCK && millis() - g_lastClockDraw >= 60000) {
    g_lastClockDraw = millis();
    renderCurrent();
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
