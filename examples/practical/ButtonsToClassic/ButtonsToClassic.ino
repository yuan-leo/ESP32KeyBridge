// Turn three switches into a Bluetooth Classic keyboard/media controller.
// Use this only for a legacy Host that accepts BR/EDR HID but not BLE HID.
// Bluetooth Classic exists only on the original ESP32 (not S3/C3/C6/P4).

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBleClassic.h>
#include <ESP32KeyBridgeGpio.h>
#include <EspBleClassic.h>

EspBleClassic bluetooth;

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::GpioKeyInputAdapter buttons;
esp32keybridge::EspBleClassicHidOutputAdapter host(bluetooth);

void setup()
{
  Serial.begin(115200);

  // The output adapter has already composed keyboard + consumer + mouse.
  // Classic publishes that descriptor through SDP when begin() runs.
  EspBleClassicConfig bluetoothConfig;
  bluetoothConfig.deviceName = "KeyBridge Classic";
  bluetoothConfig.classOfDevice.majorDeviceClass = 0x05; // Peripheral
  bluetoothConfig.classOfDevice.minorDeviceClass = 0x10; // Keyboard
  if (!bluetooth.begin(bluetoothConfig))
  {
    Serial.printf("Classic start failed: %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }

  // Switches connect these pins to GND. Change the pins for your board.
  buttons.addKey(GPIO_NUM_25, esp32keybridge::KeyboardUsage::PageUp, true, true);
  buttons.addKey(GPIO_NUM_26, esp32keybridge::KeyboardUsage::PageDown, true, true);
  buttons.addKey(GPIO_NUM_27, esp32keybridge::ConsumerUsage::PlayPause, true, true);
  bridge.addInput(buttons);
  bridge.addOutput(host);

  esp32keybridge::ESP32KeyBridgeConfig config;
  bridge.applyConfig(config);
}

void loop()
{
  bluetooth.update();
  bridge.update();
  delay(1);
}
