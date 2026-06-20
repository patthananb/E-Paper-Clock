// Claude usage payload from the Clawdmeter daemon, plus its JSON parser.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace cm {

struct UsageData {
  int  s = -1;   ///< 5h utilization %
  int  sr = 0;   ///< 5h reset, minutes
  int  w = -1;   ///< 7d utilization %
  int  wr = 0;   ///< 7d reset, minutes
  char st[16] = "";
  bool ok = false;
};

// Parse the daemon's JSON: {"s":..,"sr":..,"w":..,"wr":..,"st":"..","ok":true}
inline bool parseUsage(const String& json, UsageData& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  out.s  = doc["s"]  | -1;
  out.sr = doc["sr"] | 0;
  out.w  = doc["w"]  | -1;
  out.wr = doc["wr"] | 0;
  out.ok = doc["ok"] | false;
  const char* st = doc["st"] | "";
  strncpy(out.st, st, sizeof(out.st) - 1);
  out.st[sizeof(out.st) - 1] = 0;
  return true;
}

}  // namespace cm
