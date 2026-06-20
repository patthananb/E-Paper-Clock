// Battery gauge: reads Li-po voltage via ADC and maps to a 0-100% estimate.
#pragma once
#include <Arduino.h>

namespace cm {

class BatteryGauge {
public:
  void begin();
  int   readPercent();            ///< sample ADC and convert to %
  int   lastPercent() const { return _pct; }
  float lastVolts()   const { return _volts; }

  // No dedicated charge-status pin on this board, so this is a heuristic:
  // the cell sits high (~>4.15V) only while a charger holds it there.
  bool  isCharging()  const { return _volts >= 4.15f; }

private:
  float readVolts();
  static int  toPercent(float v);
  int   _pct   = 0;
  float _volts = 0.0f;
};

}  // namespace cm
