"""Regression tests for the Android-friendly ESP32-S3 firmware artifact."""
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
PACKAGER = ROOT / "tools" / "package_esp32s3_firmware.py"
WORKFLOW = ROOT / ".github" / "workflows" / "esp32s3-firmware.yml"
PROJECT = "UsbKeyboardToBle.ino"


def make_build(directory):
    directory.mkdir(parents=True)
    for suffix in (".merged.bin", ".bin", ".bootloader.bin", ".partitions.bin"):
        (directory / f"{PROJECT}{suffix}").write_bytes(b"\xe9" + b"testing-image")
    (directory / "boot_app0.bin").write_bytes(b"\xe9testing-boot-app")
    (directory / "flash_args").write_text(
        "--flash-mode dio --flash-freq 80m --flash-size 4MB\n"
        f"0x0 {PROJECT}.bootloader.bin\n"
        f"0x8000 {PROJECT}.partitions.bin\n"
        "0xe000 boot_app0.bin\n"
        f"0x10000 {PROJECT}.bin\n",
        encoding="utf-8",
    )


def run_packager(build, out):
    return subprocess.run(
        [sys.executable, str(PACKAGER), "--build-dir", str(build),
         "--output-dir", str(out), "--commit", "abc1234"],
        capture_output=True, text=True,
    )


def test_packager_outputs_complete_mobile_flash_bundle(tmp_path):
    build = tmp_path / "build"
    out = tmp_path / "output"
    make_build(build)
    p = run_packager(build, out)
    assert p.returncode == 0, p.stderr
    assert (out / "ESP32KeyBridge-ESP32S3-flash-0x0.bin").read_bytes() == (build / f"{PROJECT}.merged.bin").read_bytes()
    for name in (f"{PROJECT}.bin", f"{PROJECT}.bootloader.bin",
                 f"{PROJECT}.partitions.bin", "flash_args", "boot_app0.bin"):
        assert (out / name).read_bytes() == (build / name).read_bytes()
    guide = (out / "FLASHING.txt").read_text(encoding="utf-8")
    assert "0x0" in guide
    assert "0x8000" in guide
    assert "0xe000" in guide
    assert "0x10000" in guide
    assert "Android" in guide
    assert "abc1234" in guide


def test_packager_rejects_missing_firmware_file(tmp_path):
    build = tmp_path / "build"
    out = tmp_path / "output"
    make_build(build)
    (build / f"{PROJECT}.merged.bin").unlink()
    p = run_packager(build, out)
    assert p.returncode != 0
    assert ".merged.bin" in p.stderr


def test_packager_rejects_wrong_chip_flash_offsets(tmp_path):
    build = tmp_path / "build"
    out = tmp_path / "output"
    make_build(build)
    args = (build / "flash_args").read_text(encoding="utf-8")
    (build / "flash_args").write_text(args.replace("0x0 ", "0x1000 "), encoding="utf-8")
    p = run_packager(build, out)
    assert p.returncode != 0
    assert "bootloader" in p.stderr.lower()


def test_workflow_uses_pinned_s3_profile_and_publishes_artifact():
    workflow = WORKFLOW.read_text(encoding="utf-8")
    assert "workflow_dispatch:" in workflow
    assert "branches: [main]" in workflow
    assert "--profile esp32s3" in workflow
    assert "--build-path" in workflow
    assert "arduino-cli lib update-index" in workflow
    assert "package_esp32s3_firmware.py" in workflow
    assert "actions/upload-artifact@v4" in workflow
    assert "if-no-files-found: error" in workflow
