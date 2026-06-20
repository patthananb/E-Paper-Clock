#include "TimeSync.h"
#include "wifi_config.h"
#include <WiFi.h>
#include <time.h>

namespace cm {

void TimeSync::begin() {
  if (strlen(WIFI_SSID) == 0) { Serial.println("wifi: no SSID set, clock disabled"); return; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("wifi: connecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(250); Serial.print("."); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("wifi: failed"); WiFi.mode(WIFI_OFF); return; }

  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  struct tm tm;
  if (getLocalTime(&tm, 8000)) {
    _ok = true;
    Serial.printf("ntp: %04d-%02d-%02d %02d:%02d\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  } else {
    Serial.println("ntp: no time");
  }
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
}

bool TimeSync::localTime(struct tm& out) const {
  if (!_ok) return false;
  return getLocalTime(&out, 50);
}

}  // namespace cm
