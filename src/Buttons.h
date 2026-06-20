// Button input: BOOT (hold vs short press) and PWR (click). Debounced.
// poll() each loop, then drain events with the take* methods (read-and-clear).
#pragma once
#include <Arduino.h>

namespace cm {

class Buttons {
public:
  void begin();
  void poll();

  bool tookBootHold();    ///< BOOT held past the hold threshold (fires once)
  bool tookBootShort();   ///< BOOT pressed and released before the threshold
  bool tookPwrClick();    ///< PWR pressed

private:
  static constexpr uint32_t HOLD_MS  = 1000;
  static constexpr uint32_t DEBOUNCE = 40;

  int      _bootLast = HIGH;
  uint32_t _bootEdgeMs = 0, _bootPressMs = 0;
  bool     _bootHoldFired = false;
  bool     _evBootHold = false, _evBootShort = false;

  int      _pwrLast = HIGH;
  uint32_t _pwrEdgeMs = 0;
  bool     _evPwrClick = false;
};

}  // namespace cm
