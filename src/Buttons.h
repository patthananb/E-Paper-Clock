// Button input: PWR click (debounced). poll() each loop, then drain the event
// with tookPwrClick() (read-and-clear).
#pragma once
#include <Arduino.h>

namespace cm {

class Buttons {
public:
  void begin();
  void poll();
  bool tookPwrClick();    ///< PWR pressed

private:
  static constexpr uint32_t DEBOUNCE = 40;
  int      _pwrLast = HIGH;
  uint32_t _pwrEdgeMs = 0;
  bool     _evPwrClick = false;
};

}  // namespace cm
