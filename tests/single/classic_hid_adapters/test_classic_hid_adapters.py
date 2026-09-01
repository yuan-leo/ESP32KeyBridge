"""One-board smoke test for the Classic-specific adapter boundary.

This proves that the original ESP32 can link and start EspBle's bundled
Classic host, register the adapter's three-profile SDP record, start HID Host,
and run all three adapters through the bridge. Report transport needs a second
Classic-capable board and belongs in a peer test.
"""


def test_classic_hid_adapters_start_on_original_esp32(dut):
    dut.expect_exact(
        "CLASSIC_START stack=1 host=1 keyboard=1 mouse=1 consumer=1", timeout=30
    )
    dut.expect_exact(
        "CLASSIC_ADAPTERS registered=1 output_connected=0 lock_seen=0 "
        "keyboard_connected=0 mouse_connected=0 keys=0",
        timeout=30,
    )
    dut.expect_exact("CLASSIC_TEST done 1/1", timeout=10)
