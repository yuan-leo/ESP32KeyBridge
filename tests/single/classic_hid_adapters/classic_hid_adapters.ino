#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBleClassic.h>
#include <EspBleClassic.h>

EspBleClassic bluetooth;
esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspBleClassicHidHostKeyboardInputAdapter keyboard(bluetooth);
esp32keybridge::EspBleClassicHidHostMouseInputAdapter mouse(bluetooth);
esp32keybridge::EspBleClassicHidOutputAdapter output(bluetooth);

bool started = false;
bool reported = false;

void setup()
{
  Serial.begin(115200);
  delay(300);

  bridge.addInput(keyboard);
  bridge.addInput(mouse);
  bridge.addOutput(output);
  esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
  bridge.applyConfig(bridgeConfig);

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "KeyBridge Classic Adapter Test";
  classicConfig.classOfDevice.majorDeviceClass = 0x05;
  classicConfig.classOfDevice.minorDeviceClass = 0x10;

  const bool stackStarted = bluetooth.begin(classicConfig);
  const bool hostStarted = stackStarted && bluetooth.hidHost().begin();
  started = stackStarted && hostStarted;
  Serial.printf(
    "CLASSIC_START stack=%u host=%u keyboard=%u mouse=%u consumer=%u\n",
    stackStarted ? 1 : 0,
    hostStarted ? 1 : 0,
    bluetooth.hidKeyboard().configured() ? 1 : 0,
    bluetooth.hidMouse().configured() ? 1 : 0,
    bluetooth.hidConsumerControl().configured() ? 1 : 0);
  if (!started)
  {
    Serial.printf("CLASSIC_ERROR %s\n", bluetooth.lastErrorDetail().c_str());
  }
}

void loop()
{
  bluetooth.update();
  bridge.update();

  if (started && bluetooth.hidDevice().registered() && !reported)
  {
    reported = true;
    esp32keybridge::LockState lock;
    Serial.printf(
      "CLASSIC_ADAPTERS registered=%u output_connected=%u lock_seen=%u "
      "keyboard_connected=%u mouse_connected=%u keys=%u\n",
      bluetooth.hidDevice().registered() ? 1 : 0,
      output.connected() ? 1 : 0,
      output.getLockState(lock) ? 1 : 0,
      keyboard.connected() ? 1 : 0,
      mouse.connected() ? 1 : 0,
      static_cast<unsigned>(bridge.mergedKeys().count()));
  }

  // Repeat the final line because the runner can attach after setup output.
  if (reported)
  {
    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 500)
    {
      lastReport = millis();
      Serial.println("CLASSIC_TEST done 1/1");
    }
  }
  delay(1);
}
