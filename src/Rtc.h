// PCF85063A battery-backed RTC (I2C). Wire must be begun before use.
//
// Survives power-off via the board's RTC backup. Holds LOCAL time so the
// display can read it straight back; TimeSync seeds the ESP32 system clock
// from here on boot and writes the NTP-corrected time back after each sync.
#pragma once
#include <Arduino.h>

namespace cm {

class RtcClock {
public:
  bool begin();                       ///< ensure running + 24h mode; false if chip absent
  bool read(struct tm& out) const;    ///< local time; false if osc stopped / time invalid
  bool write(const struct tm& in);    ///< store local time, clears the oscillator-stop flag
};

}  // namespace cm
