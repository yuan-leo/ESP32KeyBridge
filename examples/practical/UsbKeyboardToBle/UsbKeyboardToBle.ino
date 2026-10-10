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
// Pairing and target switching: hold Scroll Lock and press top-row 1, 2, or 3
// to reconnect to that bonded host slot. Scroll Lock+0 replaces the currently
// selected slot and advertises for pairing. The Scroll Lock LED shows progress
// and returns to the host's real lock state afterwards.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <ESP32KeyBridgeEspUsbHost.h>
#include <ESP32KeyBridgeMacroSettings.h>
#include <ESP32KeyBridgePreferences.h>
#include <ESP32KeyBridgeSettings.h>
#include <ESP32KeyBridgeSettingsNavigation.h>
#include <ESP32KeyBridgeWeb.h>
#include <EspBle.h>
#include <EspUsbHost.h>

EspUsbHost usbHost; // keyboard side
EspBle ble;         // PC / phone side
constexpr char kBluetoothDeviceName[] = "KeyBridge Keyboard";

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspUsbHostKeyboardInputAdapter rawKeyboard(usbHost);
esp32keybridge::EspBleHidTargetController bluetoothTargets(ble, kBluetoothDeviceName);
esp32keybridge::SettingsNavigationInputAdapter settingsKeyboard(rawKeyboard);
esp32keybridge::BluetoothHotkeyInputAdapter keyboard(settingsKeyboard, bluetoothTargets);
esp32keybridge::EspBleHidOutputAdapter host(ble);
esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
esp32keybridge::KeyboardLayoutSettings layoutSettings;
esp32keybridge::PreferencesLayoutSettingsStorage settingsNvs;
esp32keybridge::KeyboardLayoutSettingsStore settingsStore(settingsNvs);
esp32keybridge::KeyboardMacroSettings macroSettings;
esp32keybridge::PreferencesMacroSettingsStorage macroNvs;
esp32keybridge::KeyboardMacroSettingsStore macroStore(macroNvs);
esp32keybridge::Esp32S3SettingsWebServer settingsWeb(layoutSettings, bridge);

void setup()
{
  Serial.begin(115200);

  // Keep these diagnostics in the reference sketch: USB host failures are
  // otherwise indistinguishable from BLE pairing failures at bring-up time.
  usbHost.addDeviceConnectedListener(
      [](const EspUsbHostDeviceInfo &info)
      {
        Serial.printf("USB connected: addr=%u vid=%04X pid=%04X supported=%s product=%s\n",
                      info.address, info.vid, info.pid,
                      info.supported ? "yes" : "no", info.product);
      });
  usbHost.addDeviceDisconnectedListener(
      [](const EspUsbHostDeviceInfo &info)
      {
        Serial.printf("USB disconnected: addr=%u vid=%04X pid=%04X\n",
                      info.address, info.vid, info.pid);
      });
  usbHost.addKeyboardStateListener(
      [](const EspUsbHostKeyboardState &state)
      {
        Serial.printf("USB keyboard report: addr=%u modifiers=%02X\n",
                      state.address, state.modifiers);
      });

  // 1) Start the hardware. The output adapter declared its HID profiles when
  //    it was constructed, so ble.begin() below publishes a keyboard +
  //    consumer + mouse HID service.
  const bool usbHostStarted = usbHost.begin();
  Serial.printf("USB host start: %s\n", usbHostStarted ? "ok" : "failed");

  EspBleConfig bleConfig;
  bleConfig.deviceName = kBluetoothDeviceName;
  // HID hosts expect an encrypted, bonded link.
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BLE start failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  if (!bluetoothTargets.applySelectedSlotName())
  {
    Serial.println("BLE slot name is too long");
    return;
  }
  ble.advertising().start();

  // 2) Load the persisted layout settings, then wire the bridge: USB keyboard
  // + target hotkeys in, BLE HID out. Every physical USB keyboard merged by
  // this adapter shares this engraving layout.
  esp32keybridge::LayoutSettingsError settingsError;
  const esp32keybridge::LayoutSettingsLoadStatus loadStatus =
      settingsStore.loadOrDefault(layoutSettings, &settingsError);
  Serial.printf("Layout settings load status: %u\n", static_cast<unsigned>(loadStatus));
  if (settingsError.message != nullptr)
  {
    Serial.printf("Layout settings detail: %s\n", settingsError.message);
  }
  esp32keybridge::MacroSettingsError macroError;
  const esp32keybridge::MacroSettingsLoadStatus macroLoadStatus =
      macroStore.loadOrDefault(macroSettings, &macroError);
  Serial.printf("Macro settings load status: %u\n",
                static_cast<unsigned>(macroLoadStatus));
  if (macroError.message != nullptr)
  {
    Serial.printf("Macro settings detail: %s\n", macroError.message);
  }

  esp32keybridge::InputConfig &keyboardConfig = bridgeConfig.addInputConfig();
  bridge.addInput(keyboard, keyboardConfig);
  bridge.addOutput(host);

  // 3) Hold CapsLock to turn H/J/K/L into the arrow keys (see the NavLayer
  //    example for the full explanation).
  esp32keybridge::LayerConfig &nav =
      bridgeConfig.addLayer(esp32keybridge::KeyboardUsage::CapsLock);
  nav.remap(esp32keybridge::KeyboardUsage::H, esp32keybridge::KeyboardUsage::Left);
  nav.remap(esp32keybridge::KeyboardUsage::J, esp32keybridge::KeyboardUsage::Down);
  nav.remap(esp32keybridge::KeyboardUsage::K, esp32keybridge::KeyboardUsage::Up);
  nav.remap(esp32keybridge::KeyboardUsage::L, esp32keybridge::KeyboardUsage::Right);
  if (!layoutSettings.apply(bridge, bridgeConfig, keyboardConfig, &settingsError))
  {
    Serial.printf("Layout settings apply failed: %s\n",
                  settingsError.message != nullptr ? settingsError.message : "unknown");
    return;
  }
  if (!macroSettings.apply(bridge, bridgeConfig, &macroError))
  {
    Serial.printf("Macro settings apply failed: %s\n",
                  macroError.message != nullptr ? macroError.message : "unknown");
    return;
  }

  // 4) Start the local settings website. Connect a phone to this WPA2 network,
  // open http://192.168.4.1/, and sign in as admin / configure. Replace all
  // development credentials before deploying a device.
  if (!settingsWeb.enableEditing(settingsStore, bridgeConfig, keyboardConfig))
  {
    Serial.println("Settings editing could not be enabled");
    return;
  }
  if (!settingsWeb.enableMacroEditing(macroSettings, macroStore, bridgeConfig))
  {
    Serial.println("Macro editing could not be enabled");
    return;
  }
  if (!settingsWeb.enableKeyboardNavigation(settingsKeyboard))
  {
    Serial.println("Settings keyboard navigation could not be enabled");
    return;
  }
  esp32keybridge::SettingsAccessPointConfig accessPoint;
  accessPoint.ssid = "KeyBridge-Setup";
  accessPoint.password = "keybridge";
  accessPoint.maxConnections = 1;
  accessPoint.captivePortal = true;
  accessPoint.portalUsername = "admin";
  accessPoint.portalPassword = "configure";
  esp32keybridge::SettingsWebError webError;
  if (!settingsWeb.begin(accessPoint, &webError))
  {
    Serial.printf("Settings website failed: %s\n",
                  webError.message != nullptr ? webError.message : "unknown");
  }
  else
  {
    Serial.printf("Settings Wi-Fi: %s / %s\n", accessPoint.ssid, accessPoint.password);
    Serial.printf("Portal login: %s / %s\n", accessPoint.portalUsername,
                  accessPoint.portalPassword);
    Serial.printf("Settings URL: http://%s/\n", settingsWeb.address().toString().c_str());
  }
}

void loop()
{
  // EspBle delivers its callbacks from this update(), so it has to run in the
  // same context as bridge.update() - and before it, so a report that just
  // arrived is part of this cycle.
  ble.update();
  bridge.update();
  settingsWeb.update();
  delay(1);
}
