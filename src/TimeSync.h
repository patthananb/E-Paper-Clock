// System-clock authority. On boot it seeds the ESP32 clock from the battery-
// backed PCF85063 RTC (so time survives power-off and works with no WiFi), then
// does a one-shot WiFi NTP sync and writes the corrected time back to the RTC.
// WiFi is dropped after each sync so it doesn't fight NimBLE for the radio.
#pragma once
#include <Arduino.h>
#include "Rtc.h"

namespace cm {

class TimeSync {
public:
  void begin();                          ///< seed from RTC, then WiFi+NTP, write back
  void retry() { syncNtp(); }            ///< re-run NTP only (clock already seeded)
  bool ok() const { return _ok; }        ///< have a usable time (from RTC or NTP)
  bool ntpOk() const { return _ntpOk; }  ///< last NTP sync succeeded
  bool rtcValid() const { return _rtcValid; } ///< RTC held a valid time at boot
  bool localTime(struct tm& out) const;  ///< current local time, false if !ok

private:
  bool syncNtp();    ///< WiFi up -> NTP -> write RTC -> WiFi off; true on NTP success
  void seedFromRtc();///< read RTC -> settimeofday (sets _ok when valid)

  RtcClock _rtc;
  bool _ok       = false;
  bool _ntpOk    = false;
  bool _rtcValid = false;
};

}  // namespace cm
