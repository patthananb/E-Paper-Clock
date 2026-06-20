// SHTC3 temperature sensor (I2C). Wire must be begun before use.
#pragma once
#include <Arduino.h>

namespace cm {

class TempSensor {
public:
  bool  read();                          ///< sample; true on success
  float lastC() const { return _tC; }    ///< last temperature in °C (NAN if none)

private:
  float _tC = NAN;
};

}  // namespace cm
