#include "TimeSync.h"
#include "wifi_config.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

namespace cm {

void TimeSync::begin() {
  // TZ must be set before any local<->epoch conversion (mktime/localtime).
  setenv("TZ", TZ_INFO, 1);
  tzset();

  _rtc.begin();
  seedFromRtc();   // gives a usable clock immediately, even with no WiFi
  syncNtp();       // correct it from NTP when WiFi is reachable, write back to RTC
}

// Read the battery-backed RTC and push it into the ESP32 system clock. The RTC
// holds LOCAL time, so mktime() (TZ already set) converts it to a UTC epoch.
void TimeSync::seedFromRtc() {
  struct tm tm;
  if (!_rtc.read(tm)) { Serial.println("rtc: no valid time (will wait for NTP)"); return; }

  time_t epoch = mktime(&tm);                 // local tm -> UTC epoch via TZ
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  _rtcValid = true;
  _ok = true;
  Serial.printf("rtc: seeded %04d-%02d-%02d %02d:%02d:%02d\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// One-shot WiFi NTP sync. On success, persist the corrected time to the RTC so
// the next cold boot starts accurate. Always drops WiFi before returning.
bool TimeSync::syncNtp() {
  _ntpOk = false;
  if (strlen(WIFI_SSID) == 0) { Serial.println("wifi: no SSID set, NTP skipped"); return false; }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("wifi: connecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(250); Serial.print("."); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("wifi: failed"); WiFi.mode(WIFI_OFF); return false; }

  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  struct tm tm;
  if (getLocalTime(&tm, 8000)) {
    _ntpOk = true;
    _ok = true;
    if (_rtc.write(tm)) _rtcValid = true;     // persist for the next power-cycle
    Serial.printf("ntp: %04d-%02d-%02d %02d:%02d (rtc %s)\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                  _rtcValid ? "updated" : "write failed");
  } else {
    Serial.println("ntp: no time");
  }

  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  return _ntpOk;
}

bool TimeSync::localTime(struct tm& out) const {
  if (!_ok) return false;
  return getLocalTime(&out, 50);
}

}  // namespace cm
