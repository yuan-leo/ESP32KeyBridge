#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;

void printReady()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf("DEVICE_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
}

void sendKeyboard(uint8_t modifiers, uint8_t a, uint8_t b = 0, uint8_t c = 0)
{
  EspBleHidKeyboardInputReport report;
  report.modifiers = modifiers;
  report.keys[0] = a;
  report.keys[1] = b;
  report.keys[2] = c;
  Serial.printf("DEVICE_KEYBOARD_SENT %u\n",
    bluetooth.hidKeyboard().sendReport(report) ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  EspBleClassicHidProfileConfig profile;
  profile.name = "KeyBridge Classic Peer Keyboard";
  profile.description = "KeyBridge peer";
  profile.provider = "KeyBridge";
  bluetooth.hidKeyboard().configure(profile);
  bluetooth.hidMouse().configure(profile);
  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("LED num=%u caps=%u scroll=%u\n",
        leds.numLock ? 1 : 0, leds.capsLock ? 1 : 0, leds.scrollLock ? 1 : 0);
    });
  bluetooth.hidDevice().onConnected([](const EspBleClassicHidConnection &) {
    Serial.println("DEVICE_CONNECTED");
  });
  bluetooth.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("DEVICE_DISCONNECTED");
  });
  EspBleClassicConfig config;
  config.deviceName = "KeyBridge Classic Peer Keyboard";
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  if (!bluetooth.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  printReady();
}

void loop()
{
  bluetooth.update();
  while (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'a') printReady();
    else if (command == 'k') sendKeyboard(0, 0x04);
    else if (command == 'm') sendKeyboard(0x02, 0x04);
    else if (command == 'n') sendKeyboard(0, 0x04, 0x06, 0x08);
    else if (command == 'u') sendKeyboard(0, 0, 0, 0);
    else if (command == 'l')
    {
      sendKeyboard(0, 0x39);
      delay(10);
      sendKeyboard(0, 0);
    }
    else if (command == 'o')
    {
      EspBleHidMouseReport report;
      report.buttons = 0x02;
      report.x = -9;
      report.y = 6;
      report.wheel = -1;
      Serial.printf("DEVICE_MOUSE_SENT %u\n",
        bluetooth.hidMouse().sendReport(report) ? 1 : 0);
    }
    else if (command == 'O')
    {
      EspBleHidMouseReport report;
      Serial.printf("DEVICE_MOUSE_SENT %u\n",
        bluetooth.hidMouse().sendReport(report) ? 1 : 0);
    }
  }
  delay(1);
}
