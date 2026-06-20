#include "Beeper.h"

namespace cm {

void Beeper::begin() {
  // TODO: power PA (GPIO42/46), es8311_codec_init() @ I2C 0x18, I2S setup
  // (MCK14/BCK15/LRCK38/DOUT45/DIN16, 24kHz). Needs the vendored es8311 driver.
  _ready = false;
  Serial.println("beeper: stub (ES8311 driver not integrated yet)");
}

void Beeper::beep(int ms) {
  if (!_ready) { Serial.printf("beep (%dms) [stub]\n", ms); return; }
  // TODO: write a sine/square PCM tone buffer to I2S for `ms`.
}

}  // namespace cm
