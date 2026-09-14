#!/usr/bin/bash
set -Eeuo pipefail

source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$source_root/../.." && pwd)
spec="$source_root/plasma-nabu-kcm.spec"
name=$(sed -n 's/^Name:[[:space:]]*//p' "$spec")
version=$(sed -n 's/^Version:[[:space:]]*//p' "$spec")
top=${1:-"$repo_root/dist/plasma-nabu-kcm-rpmbuild"}

mkdir -p "$top"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
archive="$top/SOURCES/$name-$version.tar.gz"
umtprd="$top/SOURCES/umtprd-1.8.1.tar.gz"
umtprd_sha256=6f57f61a1993059bcdd5f598d8c2a3888991497ef42ba3594947f00bd7746b1f

tar -czf "$archive" \
    --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
    --transform="s,^,$name-$version/," \
    -C "$repo_root" LICENSE \
    -C "$source_root" \
    API.md dbus kcm network plasma plasma-update plasma-nabu-kcm.spec polkit src/nabu-wake-control.cpp \
    src/nabu-wake-service.cpp src/nabu-sar-calibration.cpp src/nabu-usb-gadget \
    src/nabu-umtprd-start systemd tests tmpfiles translations

if [[ ! -s "$umtprd" ]]; then
    curl --fail --location --retry 3 \
        --output "$umtprd" \
        https://github.com/viveris/uMTP-Responder/archive/refs/tags/umtprd-1.8.1.tar.gz
fi
printf '%s  %s\n' "$umtprd_sha256" "$umtprd" | sha256sum --check --status

install -m0644 "$spec" "$top/SPECS/$name.spec"
rpmbuild -bs --define "_topdir $top" "$top/SPECS/$name.spec"
