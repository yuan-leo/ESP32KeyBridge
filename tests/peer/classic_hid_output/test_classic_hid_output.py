import re


def probe(device, command, pattern):
    device.write(command)
    return device.expect(pattern, timeout=20)


def test_bridge_output_reaches_classic_hid_host(dut, peers):
    host = peers["device"]
    probe(host, "a\n", re.compile(rb"HOST_READY"))
    ready = probe(
        dut,
        "a",
        re.compile(rb"DEVICE_READY address=([0-9a-f:]+) registered=1"),
    )
    address = ready.group(1)
    host.write(b"c" + address + b"\n")
    host.expect_exact("HOST_CONNECT_ACCEPTED 1", timeout=10)
    host.expect(re.compile(rb"HOST_CONNECTED peer=[0-9a-f:]+"), timeout=30)
    dut.expect_exact("DEVICE_CONNECTED", timeout=30)

    dut.write("k")
    dut.expect_exact("DEVICE_COMMAND k", timeout=10)
    host.expect_exact("KEY_STATE modifiers=00 a=1 b=0 c=0 d=0 e=0 f=0", timeout=20)
    dut.write("K")
    dut.expect_exact("DEVICE_COMMAND K", timeout=10)
    host.expect_exact("KEY_STATE modifiers=00 a=0 b=0 c=0 d=0 e=0 f=0", timeout=20)

    dut.write("c")
    dut.expect_exact("DEVICE_COMMAND c", timeout=10)
    host.expect_exact("KEY_STATE modifiers=01 a=1 b=0 c=0 d=0 e=0 f=0", timeout=20)
    dut.write("r")
    dut.expect_exact("DEVICE_COMMAND r", timeout=10)
    host.expect_exact("KEY_STATE modifiers=00 a=0 b=0 c=0 d=0 e=0 f=0", timeout=20)

    dut.write("n")
    dut.expect_exact("DEVICE_COMMAND n", timeout=10)
    host.expect_exact("KEY_STATE modifiers=00 a=1 b=1 c=1 d=1 e=1 f=1", timeout=20)
    dut.write("r")
    dut.expect_exact("DEVICE_COMMAND r", timeout=10)

    dut.write("v")
    dut.expect_exact("DEVICE_COMMAND v", timeout=10)
    consumer = host.expect(re.compile(rb"RAW id=4 hex=([0-9a-f]+)"), timeout=20)
    assert b"e900" in consumer.group(1)
    dut.write("V")
    dut.expect_exact("DEVICE_COMMAND V", timeout=10)

    dut.write("o")
    dut.expect_exact("DEVICE_COMMAND o", timeout=10)
    host.expect_exact("MOUSE buttons=02 x=-9 y=6 wheel=-1", timeout=20)
    dut.write("O")
    dut.expect_exact("DEVICE_COMMAND O", timeout=10)
    host.expect_exact("MOUSE buttons=00 x=0 y=0 wheel=0", timeout=20)

    dut.write("l")
    dut.expect_exact("DEVICE_LOCK caps=0 num=0 authority=1", timeout=10)
    host.write("l\n")
    host.expect_exact("HOST_LEDS_WRITTEN 1", timeout=10)
    dut.write("l")
    dut.expect_exact("DEVICE_LOCK caps=1 num=0 authority=1", timeout=20)

    host.write("d\n")
    host.expect_exact("HOST_DISCONNECT_ACCEPTED 1", timeout=10)
    host.expect_exact("HOST_DISCONNECTED", timeout=20)
    dut.expect_exact("DEVICE_DISCONNECTED", timeout=20)
