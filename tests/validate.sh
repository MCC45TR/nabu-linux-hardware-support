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
bash -n "$repo_root/systemd/nabu-sensor-session-gate"
bash -n "$repo_root/services/nabu-sar-service/tools/nabu-sar-capture"
bash -n "$repo_root/services/nabu-sar-service/tools/nabu-cct-iio-setup"
grep -Fq 'org.freedesktop.login1.Manager' \
    "$repo_root/services/nabu-sar-service/src/nabu-sar-service.c"
grep -Fq 'SAMPLE_STALE_USEC' \
    "$repo_root/services/nabu-sar-service/src/nabu-sar-service.c"
grep -Fq 'SSC_SENSOR_DATA_TYPE, "cct_front"' \
    "$repo_root/services/nabu-sar-service/src/nabu-cct-iio-bridge.c"
bash "$repo_root/tests/test-sensor-session-gate.sh"
grep -Fxq 'Before=display-manager.service gdm.service plasmalogin.service' \
    "$repo_root/systemd/nabu-sensor-session-gate.service"
grep -Fq -- '--sensor accelerometer --timeout 1' \
    "$repo_root/systemd/nabu-sensor-session-gate"
grep -Fxq 'wifi.wake-on-wlan=12' \
    "$repo_root/networkmanager/20-nabu-wifi-wowlan.conf"
grep -Fq '%meson_test' \
    "$repo_root/packaging/hexagonrpc-nabu/hexagonrpc.spec"
(cd "$repo_root/packaging/hexagonrpc-nabu" && sha256sum -c SOURCES.sha256)
grep -Eq '^Version:[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+$' \
    "$repo_root/packaging/hexagonrpc-nabu/hexagonrpc.spec"
grep -Fq 'rm -rf %{buildroot}%{_libdir}/systemd' \
    "$repo_root/packaging/hexagonrpc-nabu/hexagonrpc.spec"
(cd "$repo_root/packaging/libssc-nabu" && sha256sum -c SOURCES.sha256)
grep -Eq '^Version:[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+$' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq 'Provides:       libssc = %{version}-%{release}' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq 'Provides:       python3-ssc = %{version}-%{release}' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq '%meson_test' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq '0002-libssc-accept-fractional-and-integer-mount-matrices.patch' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq 'parsed_values != 9 || all_zero' \
    "$repo_root/packaging/libssc-nabu/0002-libssc-accept-fractional-and-integer-mount-matrices.patch"
grep -Fq 'else if (value->has_i)' \
    "$repo_root/packaging/libssc-nabu/0002-libssc-accept-fractional-and-integer-mount-matrices.patch"
grep -Fq '0003-ssccli-probe-arbitrary-data-types.patch' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq 'probe-data-type' \
    "$repo_root/packaging/libssc-nabu/0003-ssccli-probe-arbitrary-data-types.patch"
grep -Fq 'Z=%f rad/s' \
    "$repo_root/packaging/libssc-nabu/0003-ssccli-probe-arbitrary-data-types.patch"
grep -Fq 'BuildRequires:  /usr/bin/protoc-c' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
grep -Fq 'BuildRequires:  python3-gobject-base' \
    "$repo_root/packaging/libssc-nabu/libssc.spec"
(cd "$repo_root/packaging/iio-sensor-proxy-nabu" && sha256sum -c SOURCES.sha256)
grep -Eq '^Version:[[:space:]]+[0-9]+\.[0-9]+(\.[0-9]+)?$' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/iio-sensor-proxy.spec"
grep -Fq 'BuildRequires:  pkgconfig(libssc)' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/iio-sensor-proxy.spec"
grep -Fq 'Requires:       libssc-nabu' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/iio-sensor-proxy.spec"
grep -Fq '%meson_test' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/iio-sensor-proxy.spec"
grep -Fq '0003-udev-standardize-Nabu-SDSP-orientation.patch' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/iio-sensor-proxy.spec"
grep -Fq 'KERNEL=="fastrpc-adsp*", ENV{IIO_SENSOR_PROXY_TYPE}=""' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/0003-udev-standardize-Nabu-SDSP-orientation.patch"
grep -Fq 'drv-ssc-accel reads mount_matrix directly' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/0003-udev-standardize-Nabu-SDSP-orientation.patch"
! grep -Fq 'ENV{ACCEL_MOUNT_MATRIX}' \
    "$repo_root/packaging/iio-sensor-proxy-nabu/0003-udev-standardize-Nabu-SDSP-orientation.patch"
"$repo_root/packaging/update-upstreams.sh" --check
test -s "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
grep -Eq '^[[:space:]]*PlaybackChannels[[:space:]]+4$' \
    "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
for amplifier in BR TR BL TL; do
    grep -Fq "name='$amplifier Analog PCM Volume' 0" \
        "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
    grep -Fq "name='$amplifier PCM Source' ASP" \
        "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
    grep -Fq "name='$amplifier DSP1 Preload Switch' 0" \
        "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
    grep -Fq "name='$amplifier DRE Switch' off" \
        "$repo_root/alsa/ucm2/Xiaomi/nabu/HiFi.conf"
done
test -s "$repo_root/udev/65-rmtfs.rules"

printf '%s\n' "Hardware support validation passed."
