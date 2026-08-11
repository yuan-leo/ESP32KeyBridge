// Make a wired USB keyboard wireless: the keyboard plugs into the ESP32, and
// the PC / tablet / phone sees a Bluetooth keyboard. Everything the bridge
// can do to keys (remap, layers, layout conversion) still applies on the way
// through - this sketch keeps it to a straight pass-through plus one extra:
// CapsLock is turned into a hold-to-navigate layer, because a wireless
// keyboard is usually used somewhere without a mouse.
//
// Only one USB role is needed here (the PC side is Bluetooth, not USB), so a
// single-USB chip like the ESP32-S3 is enough - unlike the USB-to-USB
// examples, which need an ESP32-P4.
//
// Hardware: one ESP32-S3.
//   - USB Host = the native USB port: plug the keyboard in here. The board
//     must be able to supply bus power to the keyboard (a board with a USB-A
//     host connector, or a USB-OTG cable plus external 5V).
//   - Power the board from its UART port or a battery; the serial console on
//     UART0 shows the pairing state.
//
// Pairing: the device advertises as "KeyBridge Keyboard". Pair with it from
// the PC/phone Bluetooth settings once; bonding keeps it paired afterwards.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <ESP32KeyBridgeEspUsbHost.h>
#include <EspBle.h>
#include <EspUsbHost.h>

EspUsbHost usbHost; // keyboard side
EspBle ble;         // PC / phone side

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspUsbHostKeyboardInputAdapter keyboard(usbHost);
esp32keybridge::EspBleHidOutputAdapter host(ble);

void setup()
{
  Serial.begin(115200);

  // 1) Start the hardware. The output adapter declared its HID profiles when
  //    it was constructed, so ble.begin() below publishes a keyboard +
  //    consumer + mouse HID service.
  usbHost.begin();

  EspBleConfig bleConfig;
  bleConfig.deviceName = "KeyBridge Keyboard";
  // HID hosts expect an encrypted, bonded link.
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BLE start failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName(bleConfig.deviceName);
  ble.advertising().start();
  // Advertise again after the host walks away, so it can come back.
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });

  // 2) Wire the bridge: USB keyboard in, BLE HID out.
  bridge.addInput(keyboard);
  bridge.addOutput(host);

  // 3) Hold CapsLock to turn H/J/K/L into the arrow keys (see the NavLayer
  //    example for the full explanation).
  esp32keybridge::ESP32KeyBridgeConfig config;
  esp32keybridge::LayerConfig &nav =
      config.addLayer(esp32keybridge::KeyboardUsage::CapsLock);
  nav.remap(esp32keybridge::KeyboardUsage::H, esp32keybridge::KeyboardUsage::Left);
  nav.remap(esp32keybridge::KeyboardUsage::J, esp32keybridge::KeyboardUsage::Down);
  nav.remap(esp32keybridge::KeyboardUsage::K, esp32keybridge::KeyboardUsage::Up);
  nav.remap(esp32keybridge::KeyboardUsage::L, esp32keybridge::KeyboardUsage::Right);
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
