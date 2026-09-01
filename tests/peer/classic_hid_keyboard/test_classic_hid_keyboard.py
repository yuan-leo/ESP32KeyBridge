import re


def probe(device, command, pattern):
    device.write(command)
    return device.expect(pattern, timeout=20)


def test_classic_keyboard_and_mouse_drive_bridge(dut, peers):
    device = peers["device"]
    probe(dut, "a\n", re.compile(rb"HOST_READY"))
    ready = probe(
        device, "a", re.compile(rb"DEVICE_READY address=([0-9a-f:]+)")
    )
    address = ready.group(1)

    dut.write(b"c" + address + b"\n")
    dut.expect_exact("HOST_CONNECT_ACCEPTED 1", timeout=10)
    dut.expect(re.compile(rb"HOST_CONNECTED peer=[0-9a-f:]+"), timeout=30)
    device.expect_exact("DEVICE_CONNECTED", timeout=30)

    device.write("k")
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    dut.expect_exact("OUT keyboard:0005", timeout=20)  # A remapped to B

    device.write("m")
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    pressed = dut.expect(
        re.compile(rb"OUT (?=.*keyboard:0005)(?=.*keyboard:00e1).+"), timeout=20
    )
    assert pressed is not None

    device.write("n")
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    chord = dut.expect(
        re.compile(rb"OUT (?=.*keyboard:0005)(?=.*keyboard:0006)(?=.*keyboard:0008).+"),
        timeout=20,
    )
    assert chord is not None

    device.write("o")
    device.expect_exact("DEVICE_MOUSE_SENT 1", timeout=10)
    dut.expect(re.compile(rb"OUT .*mouse_button:0002"), timeout=20)
    dut.expect_exact("AXIS x=-9 y=6 wheel=-1", timeout=20)
    device.write("O")
    device.expect_exact("DEVICE_MOUSE_SENT 1", timeout=10)

    device.write("l")
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    device.expect_exact("LED num=0 caps=1 scroll=0", timeout=20)

    device.write("k")
    device.expect_exact("DEVICE_KEYBOARD_SENT 1", timeout=10)
    dut.expect_exact("OUT keyboard:0005", timeout=20)
    dut.write("d\n")
    dut.expect_exact("HOST_DISCONNECT_ACCEPTED 1", timeout=10)
    # Link state and the queued callback are reflected in adjacent update
    # cycles; neither ordering is part of the contract.
    seen = set()
    for _ in range(2):
        event = dut.expect(re.compile(rb"(OUT empty|HOST_DISCONNECTED)"), timeout=20)
        seen.add(event.group(1))
    assert seen == {b"OUT empty", b"HOST_DISCONNECTED"}
    device.expect_exact("DEVICE_DISCONNECTED", timeout=20)
