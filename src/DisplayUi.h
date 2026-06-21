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

  // Whole-screen renders. They default to a partial (no-flash) full-window
  // update; pass forceFull=true to force a true full refresh (ghost clean).
  void showStatus(const char* line, int batPct, bool forceFull = false);
  void showUsage(const UsageData& m, int batPct, bool daemonAlive, bool forceFull = false);
  void showClock(bool haveTime, const struct tm& tm, bool showHumidity,
                 float tempC, float rh, int batPct, bool forceFull = false);
  void updateClockTime(bool haveTime, const struct tm& tm);          ///< partial: HH:MM only, no flash
  void updateClockReading(bool showHumidity, float tempC, float rh); ///< partial: bottom temp/humidity line

  void refreshBattery(int batPct);     ///< partial redraw of the top-right only
  void sweepCharging(int batPct);      ///< left-to-right charging animation
};

}  // namespace cm
