#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later

import pathlib
import subprocess
import sys


helper = pathlib.Path(sys.argv[1])
source = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")


def run(*arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(helper), *arguments], text=True, capture_output=True, check=False)


assert run("apply", "0", "200", "100", "3").returncode == 64
assert run("apply", "8", "200", "100", "3").returncode == 64
assert run("apply", "5", "100", "100", "3").returncode == 64
assert run("apply", "5", "200", "100", "0").returncode == 64
assert run("apply", "5", "nan", "100", "3").returncode == 64
assert run("apply", "5", "200;touch", "100", "3").returncode == 64

assert 'constexpr auto kConfigPath = "/etc/nabu-sar.conf"' in source
assert 'QStringLiteral("nabu-sar-service.service")' in source
assert "QSaveFile" in source
assert "setDirectWriteFallback(false)" in source
assert "system(" not in source
assert "QProcess" not in source
assert "Sensor firmware" not in source or "never modified" in source

print("SAR calibration helper contract: PASS")
