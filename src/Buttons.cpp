#include "Buttons.h"
#include "Pins.h"

namespace cm {

void Buttons::begin() {
  pinMode(Pins::BOOT_BTN, INPUT_PULLUP);
  pinMode(Pins::PWR_BTN,  INPUT_PULLUP);
}

void Buttons::poll() {
  uint32_t now = millis();

  // ---- BOOT: distinguish hold from short press ----
  int boot = digitalRead(Pins::BOOT_BTN);
  if (boot != _bootLast && now - _bootEdgeMs > DEBOUNCE) {
    _bootEdgeMs = now;
    if (boot == LOW) {                       // pressed
      _bootPressMs   = now;
      _bootHoldFired = false;
    } else {                                 // released
      if (!_bootHoldFired) _evBootShort = true;
    }
    _bootLast = boot;
  }
  if (_bootLast == LOW && !_bootHoldFired && now - _bootPressMs >= HOLD_MS) {
    _bootHoldFired = true;
    _evBootHold    = true;
  }

  // ---- PWR: click ----
  int pwr = digitalRead(Pins::PWR_BTN);
  if (pwr != _pwrLast && now - _pwrEdgeMs > DEBOUNCE) {
    _pwrEdgeMs = now;
    Serial.printf("PWR pin -> %d\n", pwr);   // calibrate polarity
    if (pwr == LOW) _evPwrClick = true;      // pressed (assumed active-low)
    _pwrLast = pwr;
  }
}

bool Buttons::tookBootHold()  { bool e = _evBootHold;  _evBootHold  = false; return e; }
bool Buttons::tookBootShort() { bool e = _evBootShort; _evBootShort = false; return e; }
bool Buttons::tookPwrClick()  { bool e = _evPwrClick;  _evPwrClick  = false; return e; }

}  // namespace cm
