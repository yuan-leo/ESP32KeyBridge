// Use a Bluetooth keyboard on a machine that will not pair with one: a BIOS
// screen, a KVM, an instrument, a game console, an old PC. The ESP32 connects
// to the BLE keyboard as a Bluetooth host and appears to the PC as an
// ordinary wired USB keyboard - no drivers, no pairing on the PC side.
//
// A BLE mouse is picked up too (the same connection carries both when the
// device is a combo, and a separate mouse can be connected as well), so the
// PC also gets a plain USB mouse.
//
// Only one USB role is needed here (the keyboard side is Bluetooth, not USB),
// so a single-USB chip like the ESP32-S3 is enough.
//
// Hardware: one ESP32-S3.
//   - USB Device = the native USB port: plug this into the PC.
//   - The serial console on UART0 shows the connection state.
//
// Pairing: put the keyboard in pairing mode and reset the board. The sketch
// connects to the first connectable device advertising the HID service
// (0x1812), pairs, and bonds; after that it reconnects on its own.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <ESP32KeyBridgeEspUsbDevice.h>
#include <EspBle.h>
#include <EspUsbDevice.h>

EspBle ble;             // keyboard side
EspUsbDevice usbDevice; // PC side

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspBleHidHostKeyboardInputAdapter keyboard(ble);
esp32keybridge::EspBleHidHostMouseInputAdapter mouse(ble);
esp32keybridge::EspUsbDeviceHidOutputAdapter pc(usbDevice);

void setup()
{
  Serial.begin(115200);

  // 1) Start the hardware.
  EspUsbDeviceConfig deviceConfig;
  deviceConfig.product = "BleKeyboardToUsb";
  usbDevice.begin(deviceConfig);

  EspBleConfig bleConfig;
  bleConfig.deviceName = "KeyBridge BLE Host";
  // Commercial keyboards refuse to hand out HID reports over an unencrypted
  // link, so security is required rather than optional here.
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BLE start failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // 2) Connect to a BLE keyboard. The adapters only consume HID reports; the
  //    scan / connect / pair / discover sequence stays with the sketch, so
  //    you can filter by name or address instead of "first HID device found".
  ble.scanner().onResult(
      [](const EspBleScanResult &result)
      {
        if (result.connectable && result.advertisesService("1812"))
        {
          ble.scanner().stop();
          ble.connect(result);
        }
      });
  // HID attributes are readable only after encryption, so discovery waits
  // for pairing to finish.
  ble.onSecurityChanged(
      [](const EspBleSecurityChanged &event)
      {
        if (event.success)
        {
          ble.hidHost().discover(event.connection.id);
        }
      });
  // Hands-off reconnection: a bonded keyboard that comes back is reconnected
  // and re-discovered without another scan.
  ble.setAutoReconnect(true);
  ble.hidHost().setAutoRediscover(true);
  ble.scanner().start();

  // 3) Wire the bridge: BLE keyboard and mouse in, USB HID out. Lock state
  //    flows the other way - the PC's Caps/Num/Scroll LED report reaches the
  //    BLE keyboard's LEDs through the input adapter.
  bridge.addInput(keyboard);
  bridge.addInput(mouse);
  bridge.addOutput(pc);

  esp32keybridge::ESP32KeyBridgeConfig config;
  bridge.applyConfig(config);
}

void loop()
{
  // EspBle delivers its callbacks from this update(), so it has to run in the
  // same context as bridge.update() - and before it, so a report that just
  // arrived is part of this cycle.
  ble.update();
  bridge.update();
  delay(1);
}
