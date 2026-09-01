#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBleClassic.h>
#include <EspBleClassic.h>

EspBleClassic bluetooth;
esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspBleClassicHidHostKeyboardInputAdapter keyboard(bluetooth);
esp32keybridge::EspBleClassicHidHostMouseInputAdapter mouse(bluetooth);
esp32keybridge::ManualOutputAdapter output;
esp32keybridge::KeySet lastKeys;

bool sameKeys(const esp32keybridge::KeySet &a, const esp32keybridge::KeySet &b)
{
  if (a.count() != b.count()) return false;
  for (size_t i = 0; i < a.count(); ++i) if (!b.contains(a.at(i))) return false;
  return true;
}

void printKeys(const esp32keybridge::KeySet &keys)
{
  Serial.print("OUT");
  if (keys.count() == 0) Serial.print(" empty");
  for (size_t i = 0; i < keys.count(); ++i)
    Serial.printf(" %s:%04x", esp32keybridge::keyKindName(keys.at(i).kind), keys.at(i).code);
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bridge.addInput(keyboard);
  bridge.addInput(mouse);
  bridge.addOutput(output);
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(esp32keybridge::KeyboardUsage::A,
                      esp32keybridge::KeyboardUsage::B);
  bridge.applyConfig(config);

  bluetooth.hidHost().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HOST_CONNECTED peer=%s\n", event.peerAddress.c_str());
  });
  bluetooth.hidHost().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("HOST_DISCONNECTED");
  });
  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "KeyBridge Classic Peer Host";
  if (!bluetooth.begin(classicConfig) || !bluetooth.hidHost().begin())
  {
    Serial.printf("HOST_INIT_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("HOST_READY");
}

void loop()
{
  while (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
      Serial.printf("HOST_CONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().connect(command.c_str() + 1) ? 1 : 0);
    else if (command == "d")
      Serial.printf("HOST_DISCONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().disconnect() ? 1 : 0);
    else if (command == "a") Serial.println("HOST_READY");
  }

  bluetooth.update();
  bridge.update();
  if (!sameKeys(output.keys(), lastKeys))
  {
    lastKeys = output.keys();
    printKeys(lastKeys);
  }
  const int32_t x = bridge.axisDelta(esp32keybridge::Axis::X);
  const int32_t y = bridge.axisDelta(esp32keybridge::Axis::Y);
  const int32_t wheel = bridge.axisDelta(esp32keybridge::Axis::Wheel);
  if (x || y || wheel)
    Serial.printf("AXIS x=%d y=%d wheel=%d\n", (int)x, (int)y, (int)wheel);
  delay(1);
}
