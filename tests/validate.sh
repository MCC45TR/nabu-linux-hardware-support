#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

for profile in \
    "$repo_root/plasma/rotation-60hz/user/kwinoutputconfig.json" \
    "$repo_root/plasma/rotation-60hz/sddm/kwinoutputconfig.json" \
    "$repo_root/plasma/experimental/auto-brightness/kwinoutputconfig.json"; do
    jq empty "$profile"
done

jq -e '
    .[] | select(.name == "outputs") | .data[] |
    .connectorName == "DSI-1" and
    .autoRotation == "Always" and
    .automaticBrightness == false and
    .mode.refreshRate == 60000
' "$repo_root/plasma/rotation-60hz/user/kwinoutputconfig.json" >/dev/null

jq -e '
    .[] | select(.name == "outputs") | .data[] |
    .connectorName == "DSI-1" and
    .automaticBrightness == true and
    (.autoBrightnessCurve | length) == 11
' "$repo_root/plasma/experimental/auto-brightness/kwinoutputconfig.json" >/dev/null

bash -n "$repo_root/systemd/nabu-pmic-rtc-sync"
test -s "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
test -s "$repo_root/udev/65-rmtfs.rules"

printf '%s\n' "Hardware support validation passed."
