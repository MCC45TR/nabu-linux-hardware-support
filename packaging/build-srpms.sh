#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir=${1:-"$repo_root/dist/srpms"}
mkdir -p "$output_dir"
output_dir="$(cd -- "$output_dir" && pwd)"

for command_name in curl rpmbuild sha256sum; do
    command -v "$command_name" >/dev/null 2>&1 || {
        printf 'Missing required command: %s\n' "$command_name" >&2
        exit 2
    }
done

"$repo_root/packaging/update-upstreams.sh" --check

work_dir=$(mktemp -d /tmp/nabu-userspace-srpm.XXXXXX)
trap 'rm -rf -- "$work_dir"' EXIT

for package in hexagonrpc-nabu libssc-nabu; do
    package_dir="$repo_root/packaging/$package"
    spec="$package_dir/${package%-nabu}.spec"
    version=$(sed -n 's/^Version:[[:space:]]*//p' "$spec" | head -n 1)
    topdir="$work_dir/$package"
    mkdir -p "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
    cp -a "$package_dir"/* "$topdir/SOURCES/"
    cp -a "$spec" "$topdir/SPECS/"

    case "$package" in
        hexagonrpc-nabu)
            url="https://github.com/linux-msm/hexagonrpc/archive/refs/tags/v$version.tar.gz"
            filename="hexagonrpc-$version.tar.gz"
            ;;
        libssc-nabu)
            url="https://codeberg.org/DylanVanAssche/libssc/archive/v$version.tar.gz"
            filename="libssc-$version.tar.gz"
            ;;
    esac

    curl -fsSL --retry 3 "$url" -o "$topdir/SOURCES/$filename"
    (cd "$topdir/SOURCES" && sha256sum -c "$package_dir/UPSTREAM.sha256")
    rpmbuild -bs "$topdir/SPECS/$(basename "$spec")" \
        --define "_topdir $topdir" \
        --define "_sourcedir $topdir/SOURCES" \
        --define "_srcrpmdir $output_dir"
done

printf 'SRPMs written to %s\n' "$output_dir"
