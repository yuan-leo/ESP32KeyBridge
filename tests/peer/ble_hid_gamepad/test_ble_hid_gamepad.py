"""EspBleHidHostGamepadInputAdapter: a real BLE HID gamepad peripheral drives
the bridge input pipeline on the central board.

Buttons and the hat are mapped to keyboard keys with the same mapping the USB
gamepad peer test uses, because both adapters run on the core's shared
GamepadKeyMap - a difference between the two would be a regression in that
sharing.

One test function rather than several, because a BLE link is stateful and
expensive to set up. Bonds are cleared at both ends first and again at the
end, so a previous run's pairing state cannot change the outcome.
"""


def test_ble_gamepad_buttons_and_hat_map_to_keys(dut, peers):
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

    # --- buttons -----------------------------------------------------------
    # Button 1 -> A (0x04), button 2 -> B (0x05) via the adapter mapping;
    # unmapped buttons produce nothing.
    device.write("a")
    device.expect_exact("DEVICE_SENT BTN1_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0004", timeout=20)

    device.write("b")
    device.expect_exact("DEVICE_SENT BTN12_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0004 keyboard:0005", timeout=20)

    device.write("A")
    device.expect_exact("DEVICE_SENT BTN_UP success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- hat ---------------------------------------------------------------
    # "Up" maps to the Up arrow (0x52); centering releases it. EspBle reports
    # the hat as 0=centered plus 1-8 clockwise from up, so this also exercises
    # the nine-value branch of GamepadKeyMap::decodeHat().
    device.write("u")
    device.expect_exact("DEVICE_SENT HAT_UP success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0052", timeout=20)

    # A diagonal presses the two neighbouring directions: Right (0x4f) and
    # Up (0x52).
    device.write("e")
    device.expect_exact("DEVICE_SENT HAT_UP_RIGHT success=1", timeout=10)
    dut.expect_exact("OUT keyboard:004f keyboard:0052", timeout=20)

    device.write("c")
    device.expect_exact("DEVICE_SENT HAT_CENTER success=1", timeout=10)
    dut.expect_exact("OUT empty", timeout=20)

    # --- a disconnect releases what that input was holding ------------------
    device.write("a")
    device.expect_exact("DEVICE_SENT BTN1_DOWN success=1", timeout=10)
    dut.expect_exact("OUT keyboard:0004", timeout=20)

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
