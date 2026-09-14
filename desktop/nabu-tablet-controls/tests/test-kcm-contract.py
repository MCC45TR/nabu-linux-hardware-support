#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later

import pathlib


root = pathlib.Path(__file__).resolve().parents[1]
backend = (root / "kcm/kcm_nabu.cpp").read_text(encoding="utf-8")
qml = (root / "kcm/ui/main.qml").read_text(encoding="utf-8")
spec = (root / "plasma-nabu-kcm.spec").read_text(encoding="utf-8")

for method in (
    "setFlashlightEnabled", "setFlashlightBrightness", "setAutoRotateEnabled",
    "setAutoBrightnessEnabled", "setCoverSleepEnabled", "setDoubleTapEnabled",
    "setTiltWakeEnabled", "setGripHoldAwakeEnabled", "setUsbSharingEnabled",
    "setUsbMode", "setUsbPowerRole", "connectPen",
):
    assert method in backend, method

assert 'QStringLiteral("PropertiesChanged")' in backend
assert "startSensorLive" in backend and "stopSensorLive" in backend
assert "bus.disconnect" in backend
assert "setSingleShot(true)" in backend
assert "setInterval(20000)" in backend
assert "system(" not in backend and "popen(" not in backend
assert "/dev/sd" not in backend and "/dev/mmc" not in backend

for label in (
    "Flashlight", "Automatic rotation", "Automatic brightness", "Magnetic cover sleep",
    "Double tap to wake", "Tilt to wake", "Keep awake while held", "Xiaomi Smart Pen",
    "Pogo keyboard", "USB device sharing", "USB data mode", "USB power role",
    "Live sensors", "Grip calibration",
):
    assert label in qml, label

assert "kcm_colord" in qml and "kcm_tablet" in qml and "kcm_keyboard" in qml
assert "plasma_applet_org.senemos.nabu.flashlight" in backend
assert "nabu-sar-calibration" in spec
assert "src/nabu-sar-calibration.cpp" in spec

print("KCM parity, event and safety contract: PASS")
