#include "Buttons.h"
#include "Pins.h"

namespace cm {

void Buttons::begin() {
  pinMode(Pins::PWR_BTN, INPUT_PULLUP);
}

void Buttons::poll() {
  uint32_t now = millis();
  int pwr = digitalRead(Pins::PWR_BTN);
  if (pwr != _pwrLast && now - _pwrEdgeMs > DEBOUNCE) {
    _pwrEdgeMs = now;
    Serial.printf("PWR pin -> %d\n", pwr);   // calibrate polarity
    if (pwr == LOW) _evPwrClick = true;      // pressed (assumed active-low)
    _pwrLast = pwr;
  }
}

bool Buttons::tookPwrClick() { bool e = _evPwrClick; _evPwrClick = false; return e; }

}  // namespace cm
