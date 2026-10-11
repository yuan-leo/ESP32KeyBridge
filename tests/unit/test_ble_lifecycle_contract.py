from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
BLE = (ROOT / "src/ESP32KeyBridgeEspBle.h").read_text(encoding="utf-8")
SKETCH = (ROOT / "examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino").read_text(encoding="utf-8")
WEB = (ROOT / "src/ESP32KeyBridgeWeb.h").read_text(encoding="utf-8")


def test_boot_resumes_selected_bond_without_open_advertising():
    assert "bluetoothTargets.resumeSelectedSlot()" in SKETCH
    assert "ble.advertising().start();" not in SKETCH


def test_ble_connection_compares_bond_identity_not_rotating_ota_address():
    assert "ble_gap_conn_find" in BLE
    assert "peer_id_addr" in BLE


def test_connected_hid_link_invalidates_report_caches():
    assert "void resetSession()" in BLE
    assert "mousePending_" in BLE
    assert "ledSeen_ = false" in BLE


def test_target_switch_waits_for_encrypted_bond_and_hid_subscription():
    assert "ble_.hidKeyboard().ready()" in BLE
    assert "linkSecured_" in BLE


def test_empty_slot_never_falls_back_to_open_advertising():
    method = BLE.split("void handleDisconnected(", 1)[1].split("void handleSecurity(", 1)[0]
    assert "ble_.advertising().start()" not in method or "Pending::DirectedAdvertising" in method
    assert "suppressReconnect_" in method


def test_reset_reboot_waits_for_confirmed_bond_deletion():
    assert "reset_->completed()" in WEB
    assert "reset_->succeeded()" in WEB


def test_profile_read_failure_does_not_latch_loaded_flag():
    load = BLE.split("bool loadProfileSettings()", 1)[1].split("void update()", 1)[0]
    assert load.index("profilesLoaded_ = true;") > load.index("DefaultsStorageError")
