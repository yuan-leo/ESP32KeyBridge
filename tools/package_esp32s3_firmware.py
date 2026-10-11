#!/usr/bin/env python3
"""Package Arduino ESP32-S3 compile outputs for USB flashing from Android.

Uses the merged image emitted by Arduino-ESP32 3.3.11, rather than trying to
recreate bootloader and partition offsets independently of the board profile.
"""

import argparse
from pathlib import Path
import shlex
import shutil

PROJECT = "UsbKeyboardToBle.ino"
MERGED_NAME = "ESP32KeyBridge-ESP32S3-flash-0x0.bin"
REQUIRED = (
    f"{PROJECT}.merged.bin",
    f"{PROJECT}.bin",
    f"{PROJECT}.bootloader.bin",
    f"{PROJECT}.partitions.bin",
    "boot_app0.bin",
    "flash_args",
)
EXPECTED_OFFSETS = {
    f"{PROJECT}.bootloader.bin": 0x0,
    f"{PROJECT}.partitions.bin": 0x8000,
    "boot_app0.bin": 0xE000,
    f"{PROJECT}.bin": 0x10000,
}


def package_firmware(build_dir: Path, output_dir: Path, commit: str) -> None:
    for name in REQUIRED:
        path = build_dir / name
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Missing or empty build output: {path}")

    # ESP32-S3 loads its bootloader from address 0x0. This byte is the
    # Espressif bootloader image magic; reject a mismatched/empty image.
    with (build_dir / f"{PROJECT}.merged.bin").open("rb") as handle:
        if handle.read(1) != b"\xe9":
            raise ValueError("Merged ESP32-S3 image has an invalid bootloader header")

    offsets = {}
    for line in (build_dir / "flash_args").read_text(encoding="utf-8").splitlines():
        parts = shlex.split(line)
        if len(parts) == 2 and parts[0].lower().startswith("0x"):
            try:
                offsets[parts[1]] = int(parts[0], 16)
            except ValueError as exc:
                raise ValueError(f"Invalid flash offset: {line}") from exc

    for name, expected in EXPECTED_OFFSETS.items():
        if offsets.get(name) != expected:
            raise ValueError(
                f"Unexpected {name} flash offset: {offsets.get(name)!r}; "
                f"expected 0x{expected:x} for ESP32-S3"
            )

    output_dir.mkdir(parents=True, exist_ok=True)
    for name in REQUIRED:
        shutil.copyfile(build_dir / name, output_dir / name)
    (output_dir / f"{PROJECT}.merged.bin").rename(output_dir / MERGED_NAME)

    (output_dir / "FLASHING.txt").write_text(
        f"ESP32KeyBridge — ESP32-S3 USB-to-BLE keyboard firmware\n"
        f"Source commit: {commit}\n"
        "Build target: esp32:esp32:esp32s3:USBMode=default,PartitionScheme=min_spiffs\n"
        "Arduino-ESP32 core: 3.3.11 (pinned by sketch.yaml)\n\n"
        "RECOMMENDED: Flash from Android using a USB-OTG data connection\n"
        f"  File: {MERGED_NAME}\n"
        "  Target chip: ESP32-S3\n"
        "  Flash address: 0x0 (hexadecimal)\n"
        "  The merged image includes the bootloader, partition table, and app.\n"
        "  Do not flash this merged image at 0x10000.\n\n"
        "ALTERNATIVE: Flash individual files (advanced mode)\n"
        f"  0x0     {PROJECT}.bootloader.bin\n"
        f"  0x8000  {PROJECT}.partitions.bin\n"
        "  0xe000  boot_app0.bin\n"
        f"  0x10000 {PROJECT}.bin\n"
        "  See flash_args for the exact build-generated flash parameters.\n\n"
        "After flashing, disconnect the phone, use the S3 native USB port\n"
        "for the wired keyboard in USB-host mode, and supply suitable power\n"
        "to the board. The USB-host port must be able to power the keyboard.\n"
        "On first boot, connect to the KeyBridge-Setup Wi-Fi network and\n"
        "browse to http://192.168.4.1 to select Pair/Replace for a slot.\n"
        "Caution: flashing may overwrite existing firmware and settings.\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    try:
        package_firmware(args.build_dir, args.output_dir, args.commit)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"error: {exc}\n")
    print(f"Created ESP32-S3 flash bundle at {args.output_dir}")


if __name__ == "__main__":
    main()
