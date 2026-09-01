#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBleClassic.h>
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::ManualInputAdapter input;
esp32keybridge::EspBleClassicHidOutputAdapter output(bluetooth);
bool readyPrinted = false;

void printReady()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf("DEVICE_READY address=%02x:%02x:%02x:%02x:%02x:%02x registered=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    bluetooth.hidDevice().registered() ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  bridge.addInput(input);
  bridge.addOutput(output);
  esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
  bridge.applyConfig(bridgeConfig);

  bluetooth.hidDevice().onConnected([](const EspBleClassicHidConnection &) {
    Serial.println("DEVICE_CONNECTED");
  });
  bluetooth.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("DEVICE_DISCONNECTED");
  });
  EspBleClassicConfig config;
  config.deviceName = "KeyBridge Classic Output";
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  if (!bluetooth.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
  }
}

void loop()
{
  bluetooth.update();
  if (bluetooth.hidDevice().registered() && !readyPrinted)
  {
    readyPrinted = true;
    printReady();
  }

  while (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'a') printReady();
    else if (command == 'k') input.press(esp32keybridge::KeyboardUsage::A);
    else if (command == 'K') input.release(esp32keybridge::KeyboardUsage::A);
    else if (command == 'c')
    {
      input.press(esp32keybridge::KeyboardUsage::LeftCtrl);
      input.press(esp32keybridge::KeyboardUsage::A);
    }
    else if (command == 'n')
    {
      input.clear();
      input.press(esp32keybridge::KeyboardUsage::A);
      input.press(esp32keybridge::KeyboardUsage::B);
      input.press(esp32keybridge::KeyboardUsage::C);
      input.press(esp32keybridge::KeyboardUsage::D);
      input.press(esp32keybridge::KeyboardUsage::E);
      input.press(esp32keybridge::KeyboardUsage::F);
    }
    else if (command == 'r') input.clear();
    else if (command == 'v') input.press(esp32keybridge::ConsumerUsage::VolumeIncrement);
    else if (command == 'V') input.release(esp32keybridge::ConsumerUsage::VolumeIncrement);
    else if (command == 'o')
    {
      input.press(esp32keybridge::MouseUsage::Right);
      input.addAxisDelta(esp32keybridge::Axis::X, -9);
      input.addAxisDelta(esp32keybridge::Axis::Y, 6);
      input.addAxisDelta(esp32keybridge::Axis::Wheel, -1);
    }
    else if (command == 'O') input.release(esp32keybridge::MouseUsage::Right);
    else if (command == 'l')
    {
      const esp32keybridge::LockState &lock = bridge.lockState();
      Serial.printf("DEVICE_LOCK caps=%u num=%u authority=%u\n",
        lock.capsLock ? 1 : 0, lock.numLock ? 1 : 0,
        bridge.lockAuthorityPresent() ? 1 : 0);
    }
    Serial.printf("DEVICE_COMMAND %c\n", command);
  }
  bridge.update();
  delay(1);
}
