// Peer smoke test, device board: acts as a BLE HID keyboard + mouse +
// consumer control peripheral for the host board's bridge. Plain EspBle,
// no ESP32KeyBridge, so the test observes the input adapters and nothing
// else.
//
// Commands: x=clear bonds, ?=advertising state, a/A=key A press/release,
// m=Shift+A, k=A+C+E (multi-key), o=mouse move + right button,
// O=mouse release, v/V=VolumeUp press/release, l=CapsLock tap.

#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "ESP32KeyBridge Peer";
  keyboardConfig.productId = 0x4b42;
  if (!ble.hidKeyboard().configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(),
                  ble.lastErrorDetail().c_str());
    return;
  }
  if (!ble.hidMouse().configure() || !ble.hidConsumerControl().configure())
  {
    Serial.printf("DEVICE_COMPOSITE_CONFIG_FAILED %s %s\n", ble.lastErrorName(),
                  ble.lastErrorDetail().c_str());
    return;
  }
  // The bridge's lock state arrives here as a HID LED output report, the
  // same way a real keyboard sees it.
  ble.hidKeyboard().onOutputReport(
      [](const EspBleHidKeyboardOutputReport &report)
      {
        Serial.printf("LED numlock=%u capslock=%u scrolllock=%u\n", report.numLock ? 1 : 0,
                      report.capsLock ? 1 : 0, report.scrollLock ? 1 : 0);
      });

  EspBleConfig config;
  config.deviceName = "KeyBridge BLE Peer Keyboard";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n", ble.lastErrorName(),
                  ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection)
                  { Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id)); });
  ble.onDisconnected(
      [](const EspBleConnection &connection)
      {
        Serial.printf("DEVICE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
        ble.advertising().start();
      });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("DEVICE_READY");
}

static void sendKeyboard(const EspBleHidKeyboardReport &report, const char *label)
{
  Serial.printf("DEVICE_SENT %s success=%u\n", label,
                ble.hidKeyboard().sendReport(report) ? 1 : 0);
}

void loop()
{
  while (Serial.available() > 0)
  {
    switch (Serial.read())
    {
    case 'x':
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("DEVICE_BONDS_CLEARED success=%u count=%u\n", cleared ? 1 : 0,
                    static_cast<unsigned>(ble.bondCount()));
      break;
    }
    case '?':
      Serial.printf("DEVICE_ADVERTISING %u ready=%u\n",
                    ble.advertising().isAdvertising() ? 1 : 0,
                    ble.hidKeyboard().ready() ? 1 : 0);
      break;
    case 'a':
    {
      EspBleHidKeyboardReport report;
      report.keys[0] = 0x04; // A
      sendKeyboard(report, "A_DOWN");
      break;
    }
    case 'A':
      Serial.printf("DEVICE_SENT A_UP success=%u\n", ble.hidKeyboard().releaseAll() ? 1 : 0);
      break;
    case 'm':
    {
      EspBleHidKeyboardReport report;
      report.modifiers = EspBleHidKeyboardReport::LeftShift;
      report.keys[0] = 0x04; // Shift + A
      sendKeyboard(report, "SHIFT_A_DOWN");
      break;
    }
    case 'k':
    {
      EspBleHidKeyboardReport report;
      report.keys[0] = 0x04; // A
      report.keys[1] = 0x06; // C
      report.keys[2] = 0x08; // E
      sendKeyboard(report, "MULTI_DOWN");
      break;
    }
    case 'o':
      Serial.printf("DEVICE_SENT MOUSE success=%u\n",
                    ble.hidMouse().move(-9, 6, -1, ESP_BLE_HID_MOUSE_RIGHT) ? 1 : 0);
      break;
    case 'O':
      Serial.printf("DEVICE_SENT MOUSE_UP success=%u\n",
                    ble.hidMouse().releaseAll() ? 1 : 0);
      break;
    case 'v':
      Serial.printf("DEVICE_SENT VOLUP_DOWN success=%u\n",
                    ble.hidConsumerControl().press(0x00e9) ? 1 : 0);
      break;
    case 'V':
      Serial.printf("DEVICE_SENT VOLUP_UP success=%u\n",
                    ble.hidConsumerControl().release() ? 1 : 0);
      break;
    case 'l':
    {
      // A CapsLock tap: with no lock-reporting output on the bridge, the
      // bridge is the terminal host and answers with an LED report.
      EspBleHidKeyboardReport report;
      report.keys[0] = 0x39; // CapsLock
      const bool down = ble.hidKeyboard().sendReport(report);
      const bool up = ble.hidKeyboard().releaseAll();
      Serial.printf("DEVICE_SENT CAPS_TAP success=%u\n", (down && up) ? 1 : 0);
      break;
    }
    default:
      break;
    }
  }

  ble.update();
  delay(1);
}
