// Speaker beep via the ES8311 codec + I2S.
//
// NOTE: not yet functional. The speaker is behind the ES8311 codec (no passive
// buzzer), so a beep needs the ES8311 driver + an I2S tone buffer. Vendoring
// Waveshare's es8311 driver was blocked pending authorization, so begin()/beep()
// are stubs for now (serial log only). Pomodoro shows "TIME UP" on screen.
#pragma once
#include <Arduino.h>

namespace cm {

class Beeper {
public:
  void begin();            ///< init ES8311 + I2S (stub)
  void beep(int ms = 400); ///< play a short tone (stub)

private:
  bool _ready = false;
};

}  // namespace cm
