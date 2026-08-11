// Peer smoke test, host board: the BLE HID Host input adapters feed the
// bridge input pipeline. This board is the BLE central; the peer device
// board is the BLE HID keyboard + mouse + consumer peripheral
// (peer_device/peer_device.ino). No wiring is needed - the link is radio.
//
// Prints "OUT <kind>:<code>..." (sorted) whenever the set of keys reaching
// the output adapter changes, and "AXIS x=<dx> y=<dy> wheel=<dw>" on any
// frame that carries a nonzero relative delta.
//
// Commands: x=clear bonds, s=start scan, d=disconnect, c=report lock state.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <EspBle.h>

EspBle ble;

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspBleHidHostKeyboardInputAdapter keyboard(ble);
esp32keybridge::EspBleHidHostMouseInputAdapter mouse(ble);
esp32keybridge::ManualOutputAdapter pc;

static EspBleConnectionId connectionId = 0;
static esp32keybridge::KeySet lastKeys;

static bool sameKeys(const esp32keybridge::KeySet &a, const esp32keybridge::KeySet &b)
{
  if (a.count() != b.count())
  {
    return false;
  }
  for (size_t i = 0; i < a.count(); ++i)
  {
    if (!b.contains(a.at(i)))
    {
      return false;
    }
  }
  return true;
}

static void printKeys(const esp32keybridge::KeySet &keys)
{
  esp32keybridge::Key sorted[esp32keybridge::KeySet::MaxKeys];
  const size_t count = keys.count();
  for (size_t i = 0; i < count; ++i)
  {
    sorted[i] = keys.at(i);
  }
  for (size_t i = 1; i < count; ++i)
  {
    const esp32keybridge::Key key = sorted[i];
    size_t j = i;
    while (j > 0 && (static_cast<uint8_t>(sorted[j - 1].kind) > static_cast<uint8_t>(key.kind) ||
                     (sorted[j - 1].kind == key.kind && sorted[j - 1].code > key.code)))
    {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = key;
  }

  Serial.print("OUT");
  if (count == 0)
  {
    Serial.print(" empty");
  }
  for (size_t i = 0; i < count; ++i)
  {
    Serial.printf(" %s:%04x", esp32keybridge::keyKindName(sorted[i].kind), sorted[i].code);
  }
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "KeyBridge BLE Peer Host";
  // A HID peripheral only serves its reports over an encrypted link, so
  // security is required rather than optional here.
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
  // HID attributes are readable only after encryption, so discovery waits
  // for pairing to finish.
  ble.onSecurityChanged(
      [](const EspBleSecurityChanged &event)
      {
        if (event.success)
        {
          Serial.printf("HOST_DISCOVERY_STARTED success=%u\n",
                        ble.hidHost().discover(event.connection.id) ? 1 : 0);
        }
      });
  ble.hidHost().onDiscovered(
      [](const EspBleHidKeyboardHostDiscovery &result)
      { Serial.printf("HOST_DISCOVERED success=%u\n", result.success ? 1 : 0); });
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

  bridge.addInput(keyboard);
  bridge.addInput(mouse);
  bridge.addOutput(pc);

  // A→B proves the transform pipeline runs on BLE presses, not just that
  // the adapter saw a report.
  esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
  bridgeConfig.global.remap(esp32keybridge::KeyboardUsage::A, esp32keybridge::KeyboardUsage::B);
  bridge.applyConfig(bridgeConfig);
  Serial.println("BRIDGE_READY");
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
    case 'd':
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n",
                    ble.disconnect(connectionId) ? 1 : 0);
      break;
    case 'c':
    {
      // No lock-reporting output is registered, so the bridge is the
      // terminal host: it owns the lock state and pushes it to the
      // keyboard's LEDs itself.
      const esp32keybridge::LockState &lock = bridge.lockState();
      Serial.printf("HOST_LOCK caps=%u num=%u authority=%u\n", lock.capsLock ? 1 : 0,
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

  if (!sameKeys(pc.keys(), lastKeys))
  {
    lastKeys = pc.keys();
    printKeys(lastKeys);
  }

  const int32_t dx = bridge.axisDelta(esp32keybridge::Axis::X);
  const int32_t dy = bridge.axisDelta(esp32keybridge::Axis::Y);
  const int32_t dw = bridge.axisDelta(esp32keybridge::Axis::Wheel);
  if (dx != 0 || dy != 0 || dw != 0)
  {
    Serial.printf("AXIS x=%d y=%d wheel=%d\n", static_cast<int>(dx), static_cast<int>(dy),
                  static_cast<int>(dw));
  }

  delay(1);
}
