// Phase 7 bring-up for the ESP32-S3 local settings website.
//
// Connect a phone to the open "KeyBridge-Setup" Wi-Fi network, then open
// http://192.168.4.1/. This example has no Bluetooth controller, so its reset
// controls report unavailable instead of erasing layout or macro settings.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeMacroSettings.h>
#include <ESP32KeyBridgePreferences.h>
#include <ESP32KeyBridgeSettings.h>
#include <ESP32KeyBridgeWeb.h>

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
esp32keybridge::KeyboardLayoutSettings layoutSettings;
esp32keybridge::PreferencesLayoutSettingsStorage settingsNvs;
esp32keybridge::KeyboardLayoutSettingsStore settingsStore(settingsNvs);
esp32keybridge::KeyboardMacroSettings macroSettings;
esp32keybridge::PreferencesMacroSettingsStorage macroNvs;
esp32keybridge::KeyboardMacroSettingsStore macroStore(macroNvs);
esp32keybridge::Esp32S3SettingsWebServer settingsWeb(layoutSettings, bridge);

namespace
{
const char *loadStatusName(esp32keybridge::LayoutSettingsLoadStatus status)
{
  using Status = esp32keybridge::LayoutSettingsLoadStatus;
  switch (status)
  {
  case Status::Loaded:
    return "loaded";
  case Status::LoadedRecovered:
    return "recovered previous record";
  case Status::DefaultsNoSavedSettings:
    return "defaults (first boot)";
  case Status::DefaultsInvalidSettings:
    return "defaults (stored settings invalid)";
  case Status::DefaultsStorageError:
    return "defaults (NVS read failed)";
  }
  return "unknown";
}
} // namespace

void setup()
{
  Serial.begin(115200);

  esp32keybridge::LayoutSettingsError settingsError;
  const esp32keybridge::LayoutSettingsLoadStatus loadStatus =
      settingsStore.loadOrDefault(layoutSettings, &settingsError);
  Serial.printf("Settings: %s\n", loadStatusName(loadStatus));
  if (settingsError.message != nullptr)
  {
    Serial.printf("Settings detail: %s\n", settingsError.message);
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

  // Persisted settings are applied through the same core APIs the write
  // endpoints use. A real keyboard adapter binds to keyboardConfig.
  esp32keybridge::InputConfig &keyboardConfig = bridgeConfig.addInputConfig();
  if (!layoutSettings.apply(bridge, bridgeConfig, keyboardConfig, &settingsError))
  {
    Serial.printf("Settings apply failed: %s\n",
                  settingsError.message != nullptr ? settingsError.message : "unknown");
    return;
  }
  if (!macroSettings.apply(bridge, bridgeConfig, &macroError))
  {
    Serial.printf("Macro settings apply failed: %s\n",
                  macroError.message != nullptr ? macroError.message : "unknown");
    return;
  }
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

  esp32keybridge::SettingsAccessPointConfig accessPoint;
  accessPoint.ssid = "KeyBridge-Setup";
  accessPoint.maxConnections = 1;
  accessPoint.captivePortal = true;

  esp32keybridge::SettingsWebError webError;
  if (!settingsWeb.begin(accessPoint, &webError))
  {
    Serial.printf("Settings website failed: %s\n",
                  webError.message != nullptr ? webError.message : "unknown");
    return;
  }

  Serial.printf("Wi-Fi: %s\n", accessPoint.ssid);
  Serial.println("Network security: open");
  Serial.printf("Open: http://%s/\n", settingsWeb.address().toString().c_str());
}

void loop()
{
  // Keep keyboard processing first when integrating this into a real bridge.
  bridge.update();
  settingsWeb.update();
  delay(1);
}
