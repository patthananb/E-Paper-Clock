// E-paper UI: owns the GxEPD2 panel and renders each screen. The top-right of
// every screen carries a 4-segment battery icon, refreshed via a fast partial
// update so it doesn't full-flash.
#pragma once
#include <Arduino.h>
#include <time.h>
#include "UsageData.h"

namespace cm {

class DisplayUi {
public:
  void begin();

  void showStatus(const char* line, int batPct);
  void showUsage(const UsageData& m, int batPct, bool daemonAlive);
  void showClock(bool haveTime, const struct tm& tm, float tempC, int batPct);

  // Pomodoro: full redraw, plus a fast partial redraw of just the centre
  // (ring + countdown) for the 5s tick.
  void showPomodoro(int mm, int ss, float frac, bool active, bool timeUp, int batPct);
  void updatePomodoro(int mm, int ss, float frac, bool active, bool timeUp);

  void refreshBattery(int batPct);     ///< partial redraw of the top-right only
  void sweepCharging(int batPct);      ///< left-to-right charging animation
};

}  // namespace cm
