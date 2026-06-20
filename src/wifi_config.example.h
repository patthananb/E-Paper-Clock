// Copy this file to `wifi_config.h` and fill in your details.
// `wifi_config.h` is gitignored so your credentials never get committed.
//
//   cp src/wifi_config.example.h src/wifi_config.h
//
// Leave WIFI_SSID empty to disable WiFi/NTP — the clock then shows "WiFi down".
#pragma once

#define WIFI_SSID  ""
#define WIFI_PASS  ""

// POSIX TZ string. Default below is Thailand (UTC+7, no DST).
// UK:  "GMT0BST,M3.5.0/1,M10.5.0"   US East: "EST5EDT,M3.2.0,M11.1.0"
#define TZ_INFO    "ICT-7"

// NTP servers.
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.google.com"
