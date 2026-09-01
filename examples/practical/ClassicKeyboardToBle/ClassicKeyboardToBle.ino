// Make a legacy Bluetooth Classic keyboard visible to a BLE-only Host. Both
// radios run on one original ESP32. BLE + Classic dual-host mode is useful but
// experimental in EspBle; use it only when a single transport cannot work.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <ESP32KeyBridgeEspBleClassic.h>
#include <EspBle.h>
#include <EspBleClassic.h>

EspBle ble;                    // destination Host side
EspBleClassic bluetooth;      // legacy keyboard side

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspBleClassicHidHostKeyboardInputAdapter keyboard(bluetooth);
esp32keybridge::EspBleClassicHidHostMouseInputAdapter mouse(bluetooth);
esp32keybridge::EspBleHidOutputAdapter host(ble);

// Obtain this with EspBle's Classic/Inquiry example. Classic HID connects by
// address; there is no BLE-style service advertisement to filter.
const char *keyboardAddress = "00:00:00:00:00:00";

void setup()
{
  Serial.begin(115200);

  EspBleConfig bleConfig;
  bleConfig.deviceName = "KeyBridge BLE";
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BLE start failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName(bleConfig.deviceName);
  ble.advertising().start();
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "KeyBridge Classic Host";
  if (!bluetooth.begin(classicConfig) || !bluetooth.hidHost().begin())
  {
    Serial.printf("Classic start failed: %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  bluetooth.hidHost().connect(keyboardAddress);

  bridge.addInput(keyboard);
  bridge.addInput(mouse);
  bridge.addOutput(host);

  esp32keybridge::ESP32KeyBridgeConfig config;
  bridge.applyConfig(config);
}

void loop()
{
  bluetooth.update();
  ble.update();
  bridge.update();
  delay(1);
}
