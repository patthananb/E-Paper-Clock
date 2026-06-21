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
  void updateClockTime(bool haveTime, const struct tm& tm);  ///< partial: HH:MM only, no flash

  void refreshBattery(int batPct);     ///< partial redraw of the top-right only
  void sweepCharging(int batPct);      ///< left-to-right charging animation
};

}  // namespace cm
