#include "DisplayUi.h"
#include "Pins.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

namespace cm {

static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(Pins::EPD_CS, Pins::EPD_DC, Pins::EPD_RST, Pins::EPD_BUSY));

// Top-right battery box used by full screens and the partial refresh.
static constexpr int BAT_X = 146, BAT_Y = 6, BAT_W = 28, BAT_H = 10;

// ---- helpers ----------------------------------------------------------------

// Battery icon with `litSegs` of 4 filled (sweep animation passes raw counts).
static void drawBatteryN(int litSegs) {
  if (litSegs < 0) litSegs = 0;
  if (litSegs > 4) litSegs = 4;
  display.drawRect(BAT_X, BAT_Y, BAT_W, BAT_H, GxEPD_BLACK);
  display.fillRect(BAT_X + BAT_W, BAT_Y + 3, 2, BAT_H - 6, GxEPD_BLACK);
  const int segW = 5, gap = 1, ix = BAT_X + 1, iy = BAT_Y + 1, ih = BAT_H - 2;
  for (int i = 0; i < litSegs; i++)
    display.fillRect(ix + i * (segW + gap), iy, segW, ih, GxEPD_BLACK);
}

static void drawBattery(int batPct) {
  drawBatteryN((batPct + 12) / 25);
}

static void drawHeader(const char* title, int batPct) {
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(8, 24);
  display.print(title);
  drawBattery(batPct);
  display.drawFastHLine(0, 34, 200, GxEPD_BLACK);
}

static void drawCentered(const char* s, int y) {
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(s, 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((200 - bw) / 2 - bx, y);
  display.print(s);
}

static void fmtHM(int mins, char* out, size_t n) {
  if (mins < 0) mins = 0;
  int h = mins / 60, m = mins % 60;
  if (h > 0) snprintf(out, n, "%dh %dm", h, m);
  else       snprintf(out, n, "%dm", m);
}

static void fmtDHM(int mins, char* out, size_t n) {
  if (mins < 0) mins = 0;
  if (mins >= 24 * 60) {
    int d = mins / (24 * 60), h = (mins % (24 * 60)) / 60;
    snprintf(out, n, "%dd %dh", d, h);
  } else {
    fmtHM(mins, out, n);
  }
}

static void drawBar(int x, int y, int w, int h, int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  int fill = (w - 2) * pct / 100;
  if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, GxEPD_BLACK);
}

// ---- public -----------------------------------------------------------------

void DisplayUi::begin() {
  pinMode(Pins::EPD_PWR, OUTPUT);
  digitalWrite(Pins::EPD_PWR, LOW);          // panel ON
  delay(10);
  SPI.end();
  SPI.begin(Pins::EPD_SCK, -1, Pins::EPD_MOSI, Pins::EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(0);
}

void DisplayUi::showStatus(const char* line, int batPct) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader("Claude", batPct);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 110);
    display.print(line);
  } while (display.nextPage());
}

void DisplayUi::showUsage(const UsageData& m, int batPct, bool daemonAlive) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader("Claude", batPct);
    // daemon-alive dot: filled = recent payload, hollow = stale.
    if (daemonAlive) display.fillCircle(104, 11, 4, GxEPD_BLACK);
    else             display.drawCircle(104, 11, 4, GxEPD_BLACK);

    char rbuf[12];
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 64);  display.print("5h");
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(70, 68); display.printf("%d%%", m.s < 0 ? 0 : m.s);
    drawBar(8, 78, 184, 14, m.s);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 110);
    fmtHM(m.sr, rbuf, sizeof(rbuf));
    display.printf("reset %s", rbuf);

    display.drawFastHLine(0, 122, 200, GxEPD_BLACK);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 150);  display.print("7d");
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(70, 154); display.printf("%d%%", m.w < 0 ? 0 : m.w);
    drawBar(8, 164, 184, 14, m.w);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(8, 196);
    fmtDHM(m.wr, rbuf, sizeof(rbuf));
    display.printf("reset %s", rbuf);
  } while (display.nextPage());
}

void DisplayUi::showClock(bool haveTime, const struct tm& tm, float tempC, int batPct) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader("Clock", batPct);

    char t[16];
    if (haveTime) strftime(t, sizeof(t), "%H:%M", &tm);
    else          strcpy(t, "--:--");
    display.setFont(&FreeMonoBold24pt7b);
    drawCentered(t, 100);

    display.setFont(&FreeMonoBold12pt7b);
    if (haveTime) {
      char d[24];
      strftime(d, sizeof(d), "%a %d %b", &tm);
      drawCentered(d, 135);
    } else {
      drawCentered("WiFi down", 135);
    }

    char c[16];
    if (!isnan(tempC)) snprintf(c, sizeof(c), "%.1f C", tempC);
    else               strcpy(c, "-- C");
    display.setFont(&FreeMonoBold18pt7b);
    drawCentered(c, 185);
  } while (display.nextPage());
}

void DisplayUi::refreshBattery(int batPct) {
  display.setPartialWindow(104, 0, 96, 33);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawBattery(batPct);
  } while (display.nextPage());
}

void DisplayUi::sweepCharging(int batPct) {
  for (int k = 1; k <= 4; k++) {
    display.setPartialWindow(104, 0, 96, 33);
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      drawBatteryN(k);
    } while (display.nextPage());
    delay(120);
  }
  refreshBattery(batPct);     // settle on the real level
}

}  // namespace cm
