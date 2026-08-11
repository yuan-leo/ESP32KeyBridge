// Peer smoke test, device board: runs the bridge output pipeline
// (ManualInputAdapter -> EspBleHidOutputAdapter) so the host board can
// observe what a real Bluetooth host would receive.
//
// Commands: x=clear bonds, ?=advertising/ready state, a/A=key A press/release,
// c=Ctrl+A, k=6-key rollover (A-F), r=release all keys, v/V=VolumeUp
// press/release, o=mouse right button + wheel, O=mouse release,
// p=mouse X/Y motion, l=report the lock state the bridge holds.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <EspBle.h>

EspBle ble;

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::ManualInputAdapter keys;
esp32keybridge::EspBleHidOutputAdapter host(ble);

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "KeyBridge BLE Peer Output";
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

  bridge.addInput(keys);
  bridge.addOutput(host);

  esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
  bridge.applyConfig(bridgeConfig);
  Serial.println("DEVICE_READY");
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
                    ble.advertising().isAdvertising() ? 1 : 0, host.connected() ? 1 : 0);
      break;
    case 'a':
      keys.press(esp32keybridge::KeyboardUsage::A);
      Serial.println("DEVICE_PRESS A");
      break;
    case 'A':
      keys.release(esp32keybridge::KeyboardUsage::A);
      Serial.println("DEVICE_RELEASE A");
      break;
    case 'c':
      keys.press(esp32keybridge::KeyboardUsage::LeftCtrl);
      keys.press(esp32keybridge::KeyboardUsage::A);
      Serial.println("DEVICE_PRESS CTRL_A");
      break;
    case 'k':
      keys.press(esp32keybridge::KeyboardUsage::A);
      keys.press(esp32keybridge::KeyboardUsage::B);
      keys.press(esp32keybridge::KeyboardUsage::C);
      keys.press(esp32keybridge::KeyboardUsage::D);
      keys.press(esp32keybridge::KeyboardUsage::E);
      keys.press(esp32keybridge::KeyboardUsage::F);
      Serial.println("DEVICE_PRESS ROLLOVER");
      break;
    case 'r':
      keys.clear();
      Serial.println("DEVICE_RELEASE ALL");
      break;
    case 'v':
      keys.press(esp32keybridge::ConsumerUsage::VolumeIncrement);
      Serial.println("DEVICE_PRESS VOLUP");
      break;
    case 'V':
      keys.release(esp32keybridge::ConsumerUsage::VolumeIncrement);
      Serial.println("DEVICE_RELEASE VOLUP");
      break;
    case 'o':
      keys.press(esp32keybridge::MouseUsage::Right);
      keys.addAxisDelta(esp32keybridge::Axis::Wheel, -1);
      Serial.println("DEVICE_PRESS MOUSE_RIGHT_WHEEL");
      break;
    case 'O':
      keys.release(esp32keybridge::MouseUsage::Right);
      Serial.println("DEVICE_RELEASE MOUSE_RIGHT");
      break;
    case 'p':
      keys.addAxisDelta(esp32keybridge::Axis::X, -9);
      keys.addAxisDelta(esp32keybridge::Axis::Y, 6);
      Serial.println("DEVICE_MOVE");
      break;
    case 'l':
    {
      // The output adapter is a lock-reporting output, so the connected
      // host is the authority and the bridge only mirrors it.
      const esp32keybridge::LockState &lock = bridge.lockState();
      Serial.printf("DEVICE_LOCK caps=%u num=%u authority=%u\n", lock.capsLock ? 1 : 0,
                    lock.numLock ? 1 : 0, bridge.lockAuthorityPresent() ? 1 : 0);
      break;
    }
    default:
      break;
    }
  }

  // EspBle delivers its callbacks from this update(), so it runs in the same
  // context as bridge.update() and before it.
  ble.update();
  bridge.update();
  delay(1);
}
