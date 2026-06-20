// One-shot WiFi NTP sync. Connects, syncs the system clock, then drops WiFi
// so it doesn't fight NimBLE for the radio. System time persists while powered.
#pragma once
#include <Arduino.h>

namespace cm {

class TimeSync {
public:
  void begin();                          ///< connect + NTP, then WiFi off
  bool ok() const { return _ok; }        ///< sync succeeded (else "WiFi down")
  bool localTime(struct tm& out) const;  ///< current local time, false if !ok

private:
  bool _ok = false;
};

}  // namespace cm
