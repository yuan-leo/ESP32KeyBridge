"""BLE HID Host input adapters: a real BLE keyboard/mouse peripheral drives
the bridge input pipeline on the central board.

One test function rather than several, because a BLE link is stateful and
expensive to set up: pairing once and asserting along the way keeps the
sequence deterministic (this is how EspBle's own HID peer tests are written).
Bonds are cleared at both ends first, so a previous run's pairing state
cannot change the outcome, and again at the end so the boards are left clean.
"""


def test_ble_keyboard_and_mouse_drive_the_bridge_input_pipeline(dut, peers):
    device = peers["device"]

    # --- pair and discover -------------------------------------------------
    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)

    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=30)
    dut.expect_exact("HOST_CONNECTED id=1", timeout=30)
    device.expect_exact("DEVICE_CONNECTED id=1", timeout=30)
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=30)
    dut.expect_exact("HOST_DISCOVERED success=1", timeout=30)

    # --- a single key, through the global remap ----------------------------
    # A -> B proves the transform pipeline runs on BLE presses, not just that
    # a report arrived.
    device.write("a")
    device.expect_exact("DEVICE_SENT A_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0005", timeout=20)
    device.write("A")
    device.expect_exact("DEVICE_SENT A_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- key with a modifier ----------------------------------------------
    # Modifiers are ordinary keys inside the bridge, so Shift+A arrives as
    # two keys (LeftShift 0xe1 plus the remapped B).
    device.write("m")
    device.expect_exact("DEVICE_SENT SHIFT_A_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0005 keyboard:00e1", timeout=20)
    device.write("A")
    device.expect_exact("DEVICE_SENT A_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- several keys at once ----------------------------------------------
    # A+C+E in one report, with A remapped: proves the adapter maps the whole
    # usage snapshot and the remap applies within it.
    device.write("k")
    device.expect_exact("DEVICE_SENT MULTI_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0005 keyboard:0006 keyboard:0008", timeout=20)
    device.write("A")
    device.expect_exact("DEVICE_SENT A_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- consumer keys ride the same adapter -------------------------------
    device.write("v")
    device.expect_exact("DEVICE_SENT VOLUP_DOWN success=1", timeout=10)
    dut.expect_exact("OUT consumer:00e9", timeout=20)
    device.write("V")
    device.expect_exact("DEVICE_SENT VOLUP_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- the mouse adapter is a second input on the same bridge ------------
    # The button becomes a MouseButton key; motion becomes relative axes.
    device.write("o")
    device.expect_exact("DEVICE_SENT MOUSE success=1", timeout=10)
    dut.expect_exact("OUT mouse_button:0002", timeout=20)
    dut.expect_exact("AXIS x=-9 y=6 wheel=-1", timeout=20)
    device.write("O")
    device.expect_exact("DEVICE_SENT MOUSE_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- terminal-host lock state reaches the keyboard LEDs ----------------
    # No lock-reporting output is registered, so the bridge owns the lock
    # state: a CapsLock tap toggles it and the result is written back to the
    # BLE keyboard's LEDs.
    dut.write("c")
    dut.expect_exact("HOST_LOCK caps=0 num=0 authority=0", timeout=10)

    device.write("l")
    device.expect_exact("DEVICE_SENT CAPS_TAP success=1", timeout=10)
    device.expect_exact("LED numlock=0 capslock=1 scrolllock=0", timeout=20)
    dut.write("c")
    dut.expect_exact("HOST_LOCK caps=1 num=0 authority=0", timeout=10)

    device.write("l")
    device.expect_exact("DEVICE_SENT CAPS_TAP success=1", timeout=10)
    device.expect_exact("LED numlock=0 capslock=0 scrolllock=0", timeout=20)

    # --- a disconnect releases what that input was holding ------------------
    device.write("a")
    device.expect_exact("DEVICE_SENT A_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0005", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_DISCONNECTED id=1", timeout=30)
    dut.expect_exact("OUT empty", timeout=20)
    device.expect_exact("DEVICE_DISCONNECTED id=1", timeout=30)

    # --- leave both boards unbonded for the next run -----------------------
    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
