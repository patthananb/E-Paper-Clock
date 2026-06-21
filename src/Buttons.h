// Button input: PWR + BOOT clicks (debounced). poll() each loop, then drain an
// event with tookPwrClick() / tookBootClick() (read-and-clear).
#pragma once
#include <Arduino.h>

namespace cm {

class Buttons {
public:
  void begin();
  void poll();
  bool tookPwrClick();    ///< PWR pressed
  bool tookBootClick();   ///< BOOT short press (released < LONG_PRESS)
  bool tookBootHold();    ///< BOOT long press (held >= LONG_PRESS)

private:
  static constexpr uint32_t DEBOUNCE   = 40;
  static constexpr uint32_t LONG_PRESS = 800;   ///< hold threshold for BOOT
  int      _pwrLast = HIGH;
  uint32_t _pwrEdgeMs = 0;
  bool     _evPwrClick = false;
  int      _bootLast = HIGH;
  uint32_t _bootEdgeMs = 0;
  uint32_t _bootDownMs = 0;
  bool     _evBootClick = false;
  bool     _evBootHold = false;
};

}  // namespace cm
