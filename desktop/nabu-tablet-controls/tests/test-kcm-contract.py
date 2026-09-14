#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import pathlib
import subprocess


root = pathlib.Path(__file__).resolve().parents[1]
backend = (root / "kcm/kcm_nabu.cpp").read_text(encoding="utf-8")
qml = (root / "kcm/ui/main.qml").read_text(encoding="utf-8")
spec = (root / "plasma-nabu-kcm.spec").read_text(encoding="utf-8")
metadata = json.loads((root / "kcm/kcm_nabu.json").read_text(encoding="utf-8"))

for method in (
    "setFlashlightEnabled", "setFlashlightBrightness", "setAutoRotateEnabled",
    "setAutoBrightnessEnabled", "setCoverSleepEnabled", "setDoubleTapEnabled",
    "setTiltWakeEnabled", "setGripHoldAwakeEnabled", "setUsbSharingEnabled",
    "setUsbMode", "setUsbPowerRole", "connectPen",
):
    assert method in backend, method

assert 'QStringLiteral("PropertiesChanged")' in backend
for quality_property in (
    "SampleQuality", "DataUsable", "DataChanging",
    "ConsecutiveIdenticalSamples", "SaturatedChannelMask",
):
    assert quality_property in backend, quality_property
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
    "ADUX1050 data quality", "Identical reports", "Saturated channel mask",
):
    assert label in qml, label

assert "kcm_colord" in qml and "kcm_tablet" in qml and "kcm_keyboard" in qml
assert "plasma_applet_org.senemos.nabu.flashlight" in backend
assert "nabu-sar-calibration" in spec
assert "src/nabu-sar-calibration.cpp" in spec
assert "Requires:       nabu-sar-service >= 3.0.0-93" in spec

plugin = metadata["KPlugin"]
catalog_locales = {po.stem for po in (root / "translations").glob("*.po")}
name_locales = {key[5:-1] for key in plugin if key.startswith("Name[")}
description_locales = {key[12:-1] for key in plugin if key.startswith("Description[")}
assert name_locales == catalog_locales
assert description_locales == catalog_locales
assert plugin["Description[tr]"] == "Xiaomi Pad 5 donanım bütünleştirmesini yapılandır"

untranslated_tr = subprocess.run(
    ["msgattrib", "--untranslated", "--no-obsolete", str(root / "translations/tr.po")],
    check=True,
    capture_output=True,
    text=True,
).stdout
assert not untranslated_tr.strip(), untranslated_tr

print("KCM parity, event and safety contract: PASS")
