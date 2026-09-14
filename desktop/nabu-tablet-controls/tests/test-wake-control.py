#!/usr/bin/python3
"""Static and unprivileged contract tests for the Nabu wake controller."""

from __future__ import annotations

import pathlib
import subprocess
import sys


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve(strict=True)
    source = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")

    result = subprocess.run(
        [str(binary), "status"],
        check=True,
        capture_output=True,
        text=True,
    )
    keys = {line.partition("=")[0] for line in result.stdout.splitlines() if "=" in line}
    required = {
        "device_nabu",
        "double_tap_available",
        "double_tap_enabled",
        "tilt_wake_service",
        "tilt_wake_available",
        "tilt_wake_enabled",
        "tilt_wake_reports",
    }
    assert required <= keys

    invalid = subprocess.run(
        [str(binary), "set", "double-tap", "perhaps"],
        check=False,
        capture_output=True,
        text=True,
    )
    assert invalid.returncode == 64

    assert 'constexpr auto kDoubleTapPath = "/sys/bus/spi/devices/spi0.0/double_tap_to_wake"' in source
    assert "QSaveFile" in source
    assert "geteuid() != 0" in source
    assert "/dev/mem" not in source
    assert "system(" not in source
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
