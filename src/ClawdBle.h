// BLE peripheral matching the Clawdmeter daemon's GATT contract. Receives the
// JSON usage payload over a write characteristic. Keeps advertising until a
// daemon connects; restartAdvertising() re-opens it on demand.
#pragma once
#include <Arduino.h>

namespace cm {

class ClawdBle {
public:
  void begin();
  void restartAdvertising();

  bool isConnected() const;
  bool isAdvertising() const;

  bool takeConnEvent(bool& connected);  ///< true if connect/disconnect changed
  bool takePayload(String& out);        ///< true if a new JSON payload arrived
};

}  // namespace cm
