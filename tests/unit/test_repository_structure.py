from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def test_required_project_files_exist():
    required = [
        "README.ja.md",
        "README.md",
        "library.properties",
        "keywords.txt",
        "CHANGELOG.md",
        "src/ESP32KeyBridge.h",
        "src/ESP32KeyBridge.cpp",
        "src/ESP32KeyBridgeBluetooth.h",
        "src/ESP32KeyBridgeBluetoothSettings.h",
        "src/ESP32KeyBridgeBluetoothSettings.cpp",
        "src/ESP32KeyBridgeMacroSettings.h",
        "src/ESP32KeyBridgeMacroSettings.cpp",
        "src/ESP32KeyBridgeMacroSettingsJson.h",
        "src/ESP32KeyBridgeMacroSettingsJson.cpp",
        "src/ESP32KeyBridgeSettings.h",
        "src/ESP32KeyBridgeSettings.cpp",
        "src/ESP32KeyBridgeSettingsJson.h",
        "src/ESP32KeyBridgeSettingsJson.cpp",
        "src/ESP32KeyBridgeSettingsNavigation.h",
        "src/ESP32KeyBridgePreferences.h",
        "src/ESP32KeyBridgeReset.h",
        "src/ESP32KeyBridgeReset.cpp",
        "src/ESP32KeyBridgeStatusLed.h",
        "src/ESP32KeyBridgeWeb.h",
        "src/ESP32KeyBridgeEspBle.h",
        "examples/README.ja.md",
        "examples/README.md",
        "docs/REQUIREMENTS.ja.md",
        "docs/SCOPE.md",
        "docs/SETTINGS.md",
        "docs/CORE_DESIGN.ja.md",
        "docs/ADAPTERS.ja.md",
        "docs/DATA_MODEL.ja.md",
        "docs/USE_CASES.ja.md",
        "docs/DECISIONS.ja.md",
        "docs/API_SKETCHES.ja.md",
        "docs/CONFIGURATION.ja.md",
        "docs/DEVELOPMENT_PLAN.ja.md",
        "docs/RELEASE_CHECKLIST.ja.md",
        "tests/README.ja.md",
        "tests/TEST_PLAN.ja.md",
        "tests/unit/core_smoke/core_smoke.ino",
        "tests/unit/core_smoke/sketch.yaml",
        "tests/unit/core_smoke/test_core_smoke.py",
        "tests/single/README.ja.md",
        "tests/peer/README.ja.md",
        "tests/peer/README.md",
        "tests/manual/README.ja.md",
        "tests/manual/README.md",
    ]

    missing = [path for path in required if not (ROOT / path).exists()]

    assert missing == []


def test_library_properties_names_public_header():
    library_properties = (ROOT / "library.properties").read_text(encoding="utf-8")

    assert "name=ESP32KeyBridge" in library_properties
    assert "includes=ESP32KeyBridge.h" in library_properties


def readme_section(readme, title):
    return readme.split(f"## {title}", 1)[1].split("\n## ", 1)[0]


def test_examples_readmes_list_current_examples():
    example_dirs = sorted(
        path.parent.name for path in (ROOT / "examples" / "practical").glob("*/*.ino")
    )
    for readme_name in ["README.ja.md", "README.md"]:
        readme = (ROOT / "examples" / readme_name).read_text(encoding="utf-8")
        listed = sorted(re.findall(r"- `([^`]+)`:", readme_section(readme, "Practical Examples")))

        assert listed == example_dirs, readme_name


def test_examples_are_grouped_with_docs_and_compile_profiles():
    missing = []
    for ino in sorted((ROOT / "examples").rglob("*.ino")):
        example_dir = ino.parent
        assert example_dir.parent.name == "practical", str(example_dir)
        for required in ["README.ja.md", "sketch.yaml"]:
            if not (example_dir / required).exists():
                missing.append(str((example_dir / required).relative_to(ROOT)))

    assert missing == []


def test_settings_portal_is_open_and_exposes_recovery_and_reset_controls():
    web = (ROOT / "src/ESP32KeyBridgeWeb.h").read_text(encoding="utf-8")

    assert 'server_.on("/", HTTP_GET' in web
    assert 'server_.on("/api/v1/config", HTTP_GET' in web
    assert 'server_.on("/api/v1/config", HTTP_PUT' in web
    assert 'server_.on("/api/v1/layouts", HTTP_GET' in web
    assert 'server_.on("/api/v1/navigation", HTTP_GET' in web
    assert 'server_.on("/api/v1/macros", HTTP_GET' in web
    assert 'server_.on("/api/v1/macros", HTTP_PUT' in web
    assert 'server_.on("/api/v1/status", HTTP_GET' in web
    assert 'server_.on("/api/v1/bluetooth", HTTP_GET' in web
    assert 'server_.on("/api/v1/bluetooth/label", HTTP_PUT' in web
    assert 'server_.on("/api/v1/bluetooth/action", HTTP_POST' in web
    assert 'server_.on("/api/v1/reset", HTTP_POST' in web
    assert 'server_.on("/healthz", HTTP_GET' in web
    assert web.count("HTTP_POST") == 2
    assert web.count("HTTP_PUT") == 3
    assert "https://" not in web
    assert 'src="http' not in web
    assert 'href="http' not in web
    assert "#include <DNSServer.h>" in web
    assert 'dnsServer_.start(53, "*", WiFi.softAPIP())' in web
    assert 'server_.uri() != "/api"' in web
    assert '!server_.uri().startsWith("/api/")' in web
    assert 'String("http://") + WiFi.softAPIP().toString()' in web
    for forbidden in ["authenticate", "requestAuthentication", "DIGEST_AUTH", "BASIC_AUTH", "portalUsername", "portalPassword"]:
        assert forbidden not in web
    assert "WiFi.softAP(config.ssid, nullptr" in web
    assert "Bluetooth Profiles" in web
    assert "Cancel Pairing" in web
    assert "Pair / Replace" in web
    assert "setTimeout(pollBluetooth,1000)" in web
    assert "aria-live=\"polite\"" in web
    assert "root.contains(document.activeElement)" in web
    assert "Stop / Force disconnect" in web
    assert "Not reported by host" in web
    assert "Bluetooth only" in web
    assert "Reset everything" in web
    assert "This erases Bluetooth bonds and profiles only" in web
    assert "This erases Bluetooth, layouts, and macros" in web
    assert "LED legend" in web
    assert 'action == "stop"' in web
    assert "hostName" in web and "appendJsonString(body, p.hostName)" in web
    assert "bluetoothStatusLedPatternName" in web
    assert "reset_already_in_progress" in web


def test_usb_keyboard_to_ble_wires_persistent_profile_management():
    sketch = (ROOT / "examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino").read_text(
        encoding="utf-8"
    )
    controller = (ROOT / "src/ESP32KeyBridgeEspBle.h").read_text(encoding="utf-8")

    assert "PreferencesBluetoothProfileStorage" in sketch
    assert "BluetoothProfileSettingsStore" in sketch
    assert "loadProfileSettings" in sketch
    assert "enableBluetoothManagement" in sketch
    assert "BluetoothProfileSettingsStore *profileStore_" in controller
    assert "bool cancelPairing() override" in controller
    assert "bool deleteSlot(size_t slot) override" in controller


def test_ble_profile_recovery_has_deadlines_reset_and_remote_name_discovery():
    controller = (ROOT / "src/ESP32KeyBridgeEspBle.h").read_text(encoding="utf-8")
    interface = (ROOT / "src/ESP32KeyBridgeBluetooth.h").read_text(encoding="utf-8")

    assert "ReconnectTimeoutMs = 30000" in controller
    assert "PairingTimeoutMs = 120000" in controller
    assert "bool forceDisconnect() override" in controller
    assert "bool resetBluetooth() override" in controller
    assert "deleteAllBonds()" in controller
    assert "addCharacteristicDiscoveredListener" in controller
    assert "addCharacteristicReadListener" in controller
    assert "removeGattListener" in controller
    assert 'discoverCharacteristic(peripheralConnectionId_, "1800", "2a00"' in controller
    assert "result.connectionId != peripheralConnectionId_" in controller
    assert "equalsIgnoreCase(registry_.profile(selectedSlot_).address)" in controller
    assert "class BluetoothOperationDeadline" in interface


def test_hw678_status_led_uses_only_the_gpio48_rgb_led():
    led = (ROOT / "src/ESP32KeyBridgeStatusLed.h").read_text(encoding="utf-8")
    sketch = (ROOT / "examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino").read_text(
        encoding="utf-8"
    )

    for pattern in ["Boot", "Connected", "Searching", "Pairing", "Disconnected", "Destructive", "Error"]:
        assert pattern in led
    assert "MaximumChannel = 32" in led
    assert "RGB_BUILTIN" in sketch
    assert "GPIO 48" in sketch
    assert "rgbLedWrite(RGB_BUILTIN" in sketch
    assert "PWR" in sketch and "TX" in sketch and "RX" in sketch
