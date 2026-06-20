#include "ClawdBle.h"
#include <NimBLEDevice.h>

namespace cm {

// ---- GATT contract (must match the daemon) ----
static const char* DEVICE_NAME  = "Clawdmeter";
static const char* SERVICE_UUID = "4c41555a-4465-7669-6365-000000000001";
static const char* RX_CHAR_UUID = "4c41555a-4465-7669-6365-000000000002"; // daemon -> us
static const char* REQ_CHAR_UUID= "4c41555a-4465-7669-6365-000000000004"; // us -> daemon

// ---- state shared between the NimBLE task and loop() ----
static volatile bool g_haveNew     = false;
static volatile bool g_connEvent   = false;
static volatile bool g_connected   = false;
static volatile bool g_advertising = false;
static String        g_payload;
static portMUX_TYPE  g_mux = portMUX_INITIALIZER_UNLOCKED;

static void startAdv() {
  NimBLEDevice::startAdvertising();
  g_advertising = true;
  Serial.printf("advertising as %s\n", DEVICE_NAME);
}

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    portENTER_CRITICAL(&g_mux);
    g_payload = String(c->getValue().c_str());
    g_haveNew = true;
    portEXIT_CRITICAL(&g_mux);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    g_connected = true; g_connEvent = true; g_advertising = false;
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    g_connected = false; g_connEvent = true;
    startAdv();
  }
};

void ClawdBle::begin() {
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic* rx = svc->createCharacteristic(
      RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  svc->createCharacteristic(REQ_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();

  // 128-bit UUID + flags nearly fill the 31B advert; name goes in scan response
  // so macOS (which matches by name) can discover it.
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));
  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->enableScanResponse(true);

  startAdv();
}

void ClawdBle::restartAdvertising() { startAdv(); }
bool ClawdBle::isConnected()   const { return g_connected; }
bool ClawdBle::isAdvertising() const { return g_advertising; }

bool ClawdBle::takeConnEvent(bool& connected) {
  if (!g_connEvent) return false;
  g_connEvent = false;
  connected = g_connected;
  return true;
}

bool ClawdBle::takePayload(String& out) {
  if (!g_haveNew) return false;
  portENTER_CRITICAL(&g_mux);
  out = g_payload;
  g_haveNew = false;
  portEXIT_CRITICAL(&g_mux);
  return true;
}

}  // namespace cm
