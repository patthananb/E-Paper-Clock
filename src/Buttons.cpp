#include "Buttons.h"
#include "Pins.h"

namespace cm {

void Buttons::begin() {
  pinMode(Pins::PWR_BTN, INPUT_PULLUP);
  pinMode(Pins::BOOT_BTN, INPUT_PULLUP);
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
  int boot = digitalRead(Pins::BOOT_BTN);     // active-low, GPIO0
  if (boot != _bootLast && now - _bootEdgeMs > DEBOUNCE) {
    _bootEdgeMs = now;
    if (boot == LOW) {                         // pressed: start timing the hold
      _bootDownMs = now;
    } else {                                   // released: short click vs long hold
      if (now - _bootDownMs >= LONG_PRESS) _evBootHold = true;
      else                                 _evBootClick = true;
    }
    _bootLast = boot;
  }
}

bool Buttons::tookPwrClick()  { bool e = _evPwrClick;  _evPwrClick  = false; return e; }
bool Buttons::tookBootClick() { bool e = _evBootClick; _evBootClick = false; return e; }
bool Buttons::tookBootHold()  { bool e = _evBootHold;  _evBootHold  = false; return e; }

}  // namespace cm
