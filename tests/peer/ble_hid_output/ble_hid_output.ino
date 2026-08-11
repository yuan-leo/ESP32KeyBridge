// Peer smoke test, host board: observes what the peer device board's bridge
// emits through EspBleHidOutputAdapter. This board is a plain EspBle HID
// Host (no ESP32KeyBridge), so what it prints is what a real Bluetooth host
// would receive. The peer device board runs the bridge output pipeline
// (peer_device/peer_device.ino). No wiring is needed - the link is radio.
//
// Commands: x=clear bonds, s=start scan, l=write CapsLock+NumLock LEDs,
// d=disconnect.

#include <EspBle.h>

EspBle ble;

static EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result)
                             { Serial.printf("HOST_DISCOVERED success=%u\n", result.success ? 1 : 0); });
  // The keyboard usage snapshot: report-format independent, so it does not
  // matter that the bridge sends 6KRO boot-shaped reports.
  ble.hidHost().onKeyboardState(
      [](const EspBleHidKeyboardState &state)
      {
        Serial.printf("KEY_STATE modifiers=%02x a=%u b=%u c=%u d=%u e=%u f=%u\n", state.modifiers,
                      state.isDown(0x04) ? 1 : 0, state.isDown(0x05) ? 1 : 0,
                      state.isDown(0x06) ? 1 : 0, state.isDown(0x07) ? 1 : 0,
                      state.isDown(0x08) ? 1 : 0, state.isDown(0x09) ? 1 : 0);
      });
  ble.hidHost().onConsumerControl([](const EspBleHidConsumerControlEvent &event)
                                  { Serial.printf("CONSUMER usage=%04x pressed=%u released=%u\n", event.usage,
                                                  event.pressed ? 1 : 0, event.released ? 1 : 0); });
  ble.hidHost().onMouse([](const EspBleHidMouseEvent &event)
                        { Serial.printf("MOUSE buttons=%02x x=%d y=%d wheel=%d\n", event.buttons, event.x,
                                        event.y, event.wheel); });

  EspBleConfig config;
  config.deviceName = "KeyBridge BLE Output Observer";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected(
      [](const EspBleConnection &connection)
      {
        connectionId = connection.id;
        Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
      });
  ble.onSecurityChanged(
      [](const EspBleSecurityChanged &event)
      {
        if (event.success)
        {
          Serial.printf("HOST_DISCOVERY_STARTED success=%u\n",
                        ble.hidHost().discover(event.connection.id) ? 1 : 0);
        }
      });
  ble.onDisconnected(
      [](const EspBleConnection &connection)
      {
        connectionId = 0;
        Serial.printf("HOST_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
      });
  ble.scanner().onResult(
      [](const EspBleScanResult &result)
      {
        if (result.connectable && result.advertisesService("1812"))
        {
          ble.scanner().stop();
          Serial.printf("HOST_CONNECT_STARTED success=%u\n", ble.connect(result) ? 1 : 0);
        }
      });
  Serial.println("HOST_READY");
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
      Serial.printf("HOST_BONDS_CLEARED success=%u count=%u\n", cleared ? 1 : 0,
                    static_cast<unsigned>(ble.bondCount()));
      break;
    }
    case 's':
      Serial.printf("HOST_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
      break;
    case 'l':
      // Becomes a HID LED output report: the bridge's output adapter is a
      // lock-reporting output, so this board is the lock authority.
      Serial.printf("HOST_LEDS_WRITTEN success=%u\n",
                    ble.hidHost().setKeyboardLeds(connectionId, true, true, false) ? 1 : 0);
      break;
    case 'd':
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n",
                    ble.disconnect(connectionId) ? 1 : 0);
      break;
    default:
      break;
    }
  }

  ble.update();
  delay(1);
}
