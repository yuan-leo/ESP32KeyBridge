"""EspBleHidOutputAdapter: the bridge output pipeline emits HID over GATT and
a plain EspBle HID Host observes it.

One test function rather than several, because a BLE link is stateful and
expensive to set up. Bonds are cleared at both ends first and again at the
end, so a previous run's pairing state cannot change the outcome.
"""


def test_bridge_output_reaches_a_real_ble_hid_host(dut, peers):
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

    # connected() means "subscribed and ready to notify", not just linked.
    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 0 ready=1", timeout=20)

    # --- a single key ------------------------------------------------------
    device.write("a")
    device.expect_exact("DEVICE_PRESS A", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=00 a=1 b=0 c=0 d=0 e=0 f=0", timeout=20)
    device.write("A")
    device.expect_exact("DEVICE_RELEASE A", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=00 a=0 b=0 c=0 d=0 e=0 f=0", timeout=20)

    # --- a modifier is packed into the modifier byte -----------------------
    device.write("c")
    device.expect_exact("DEVICE_PRESS CTRL_A", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=01 a=1 b=0 c=0 d=0 e=0 f=0", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE ALL", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=00 a=0 b=0 c=0 d=0 e=0 f=0", timeout=20)

    # --- six keys fill the boot report exactly -----------------------------
    device.write("k")
    device.expect_exact("DEVICE_PRESS ROLLOVER", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=00 a=1 b=1 c=1 d=1 e=1 f=1", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE ALL", timeout=10)
    dut.expect_exact("KEY_STATE modifiers=00 a=0 b=0 c=0 d=0 e=0 f=0", timeout=20)

    # --- consumer usages go out on their own report ------------------------
    device.write("v")
    device.expect_exact("DEVICE_PRESS VOLUP", timeout=10)
    dut.expect_exact("CONSUMER usage=00e9 pressed=1", timeout=20)
    device.write("V")
    device.expect_exact("DEVICE_RELEASE VOLUP", timeout=10)
    dut.expect_exact("CONSUMER usage=0000", timeout=20)

    # --- mouse buttons, wheel, and motion ----------------------------------
    device.write("o")
    device.expect_exact("DEVICE_PRESS MOUSE_RIGHT_WHEEL", timeout=10)
    dut.expect_exact("MOUSE buttons=02 x=0 y=0 wheel=-1", timeout=20)
    device.write("p")
    device.expect_exact("DEVICE_MOVE", timeout=10)
    dut.expect_exact("MOUSE buttons=02 x=-9 y=6 wheel=0", timeout=20)
    device.write("O")
    device.expect_exact("DEVICE_RELEASE MOUSE_RIGHT", timeout=10)
    dut.expect_exact("MOUSE buttons=00 x=0 y=0 wheel=0", timeout=20)

    # --- the host's LED report makes it the lock authority -----------------
    device.write("l")
    device.expect_exact("DEVICE_LOCK caps=0 num=0 authority=1", timeout=10)
    dut.write("l")
    dut.expect_exact("HOST_LEDS_WRITTEN success=1", timeout=20)
    device.write("l")
    device.expect_exact("DEVICE_LOCK caps=1 num=1 authority=1", timeout=20)

    # --- leave both boards unbonded for the next run -----------------------
    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_DISCONNECTED id=1", timeout=30)
    device.expect_exact("DEVICE_DISCONNECTED id=1", timeout=30)

    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
