#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail

package_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$package_root/../.." && pwd)"
service_root="$repo_root/services/nabu-hardware-provenance"
topdir=${1:-"$package_root/rpmbuild"}
version=$(sed -n 's/^Version:[[:space:]]*//p' \
    "$package_root/nabu-hardware-provenance.spec" | head -n 1)
source_date_epoch=${SOURCE_DATE_EPOCH:-$(git -C "$repo_root" log -1 --format=%ct)}
staging=$(mktemp -d)
trap 'rm -rf -- "$staging"' EXIT

mkdir -p "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
archive_root="$staging/nabu-hardware-provenance-$version"
install -d "$archive_root/tests"
install -m0644 "$service_root/nabu-hardware-provenance.cpp" "$archive_root/"
install -m0644 \
    "$service_root/nabu-hardware-provenance.service" \
    "$service_root/60-nabu-hardware-provenance.preset" \
    "$service_root/70-nabu-hardware-provenance.rules" \
    "$service_root/nabu-hardware-provenance.sysusers" \
    "$service_root/README.md" \
    "$repo_root/LICENSE" \
    "$archive_root/"
install -m0644 "$service_root/tests/test_provenance.py" "$archive_root/tests/"

tar --sort=name \
    --mtime="@$source_date_epoch" \
    --owner=0 --group=0 --numeric-owner \
    -C "$staging" -czf \
    "$topdir/SOURCES/nabu-hardware-provenance-$version.tar.gz" \
    "nabu-hardware-provenance-$version"
install -m0644 "$package_root/nabu-hardware-provenance.spec" "$topdir/SPECS/"
install -m0644 "$service_root/nabu-hardware-provenance.sysusers" \
    "$topdir/SOURCES/"

rpmbuild -bs \
    --define "_topdir $topdir" \
    "$topdir/SPECS/nabu-hardware-provenance.spec"
