#!/usr/bin/bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
libssc=$root/../libssc-nabu
proxy=$root/../iio-sensor-proxy-nabu
top=${1:-$root/rpmbuild}
mkdir -p "$top"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

curl -fsSL --retry 3 \
    https://codeberg.org/DylanVanAssche/libssc/archive/v0.4.4.tar.gz \
    -o "$top/SOURCES/libssc-0.4.4.tar.gz"
curl -fsSL --retry 3 \
    https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/-/archive/3.9/iio-sensor-proxy-3.9.tar.gz \
    -o "$top/SOURCES/iio-sensor-proxy-3.9.tar.gz"

for patch in "$libssc"/*.patch; do
    install -m0644 "$patch" "$top/SOURCES/libssc-${patch##*/}"
done
for patch in "$proxy"/*.patch; do
    install -m0644 "$patch" "$top/SOURCES/iio-${patch##*/}"
done

(
    cd "$top/SOURCES"
    sha256sum -c "$libssc/UPSTREAM.sha256"
    sha256sum -c "$proxy/UPSTREAM.sha256"
    find . -maxdepth 1 -type f -name 'libssc-*.patch' -printf '%f\n' \
        | LC_ALL=C sort | xargs sha256sum >nabu-sensors-patches.sha256
    find . -maxdepth 1 -type f -name 'iio-*.patch' -printf '%f\n' \
        | LC_ALL=C sort | xargs sha256sum >>nabu-sensors-patches.sha256
    sha256sum -c nabu-sensors-patches.sha256
)

install -m0644 "$root/nabu-sensors.spec" "$top/SPECS/"
rpmbuild -bs --define "_topdir $top" "$top/SPECS/nabu-sensors.spec"
