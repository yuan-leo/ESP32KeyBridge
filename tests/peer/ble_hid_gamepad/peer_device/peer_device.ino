// Peer smoke test, device board: acts as a BLE HID gamepad peripheral for
// the host board's bridge. Plain EspBle, no ESP32KeyBridge, so the test
// observes the gamepad input adapter and nothing else.
//
// EspBle's gamepad report declares buttons 1-32 and a hat switch with a
// logical range of 0-8, where 0 is an explicit "centered" and 1-8 are the
// eight directions clockwise from up. That is the nine-value encoding
// GamepadKeyMap::decodeHat() handles, so this test also covers the decoding
// path shared with the USB gamepad adapter.
//
// Commands: x=clear bonds, ?=advertising state, a=button 1, b=buttons 1+2,
// A=release buttons, u=hat up, e=hat up-right (diagonal), c=hat centered.

#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleHidGamepadConfig gamepadConfig;
  gamepadConfig.manufacturer = "ESP32KeyBridge Peer";
  gamepadConfig.productId = 0x4b47;
  if (!ble.hidGamepad().configure(gamepadConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(),
                  ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "KeyBridge BLE Peer Gamepad";
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

// Sticks and triggers stay centered: only the buttons and the hat are mapped
// on the bridge side, so everything else has to stay out of the way.
static void sendPad(uint8_t hat, uint32_t buttons, const char *label)
{
  Serial.printf("DEVICE_SENT %s success=%u\n", label,
                ble.hidGamepad().send(0, 0, 0, 0, 0, 0, hat, buttons) ? 1 : 0);
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
                    ble.hidGamepad().ready() ? 1 : 0);
      break;
    case 'a':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0x00000001u, "BTN1_DOWN");
      break;
    case 'b':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0x00000003u, "BTN12_DOWN");
      break;
    case 'A':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0, "BTN_UP");
      break;
    case 'u':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_UP, 0, "HAT_UP");
      break;
    case 'e':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_UP_RIGHT, 0, "HAT_UP_RIGHT");
      break;
    case 'c':
      sendPad(ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0, "HAT_CENTER");
      break;
    default:
      break;
    }
  }

  ble.update();
  delay(1);
}
