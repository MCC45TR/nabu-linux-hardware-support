#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode=check

if [[ ${1:-} == --check || ${1:-} == --update ]]; then
    mode=${1#--}
    shift
fi

known_packages=(hexagonrpc-nabu libssc-nabu iio-sensor-proxy-nabu)
packages=("${@:-}")
if (( ${#packages[@]} == 0 )) || [[ -z ${packages[0]} ]]; then
    packages=("${known_packages[@]}")
fi

for command_name in curl git patch sha256sum sort tar; do
    command -v "$command_name" >/dev/null 2>&1 || {
        printf 'Missing required command: %s\n' "$command_name" >&2
        exit 2
    }
done

declare -A upstream_repo tag_prefix source_url archive_name source_root release_value
declare -A target_version target_sha current_version

upstream_repo[hexagonrpc-nabu]=https://github.com/linux-msm/hexagonrpc.git
tag_prefix[hexagonrpc-nabu]=v
source_url[hexagonrpc-nabu]=https://github.com/linux-msm/hexagonrpc/archive/refs/tags/vVERSION.tar.gz
archive_name[hexagonrpc-nabu]=hexagonrpc-VERSION.tar.gz
source_root[hexagonrpc-nabu]=hexagonrpc-VERSION
release_value[hexagonrpc-nabu]='1.nabu1.test%{?dist}'

upstream_repo[libssc-nabu]=https://codeberg.org/DylanVanAssche/libssc.git
tag_prefix[libssc-nabu]=v
source_url[libssc-nabu]=https://codeberg.org/DylanVanAssche/libssc/archive/vVERSION.tar.gz
archive_name[libssc-nabu]=libssc-VERSION.tar.gz
source_root[libssc-nabu]=libssc
release_value[libssc-nabu]='1.nabu1.test%{?dist}'

upstream_repo[iio-sensor-proxy-nabu]=https://gitlab.freedesktop.org/hadess/iio-sensor-proxy.git
tag_prefix[iio-sensor-proxy-nabu]=''
source_url[iio-sensor-proxy-nabu]=https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/-/archive/VERSION/iio-sensor-proxy-VERSION.tar.gz
archive_name[iio-sensor-proxy-nabu]=iio-sensor-proxy-VERSION.tar.gz
source_root[iio-sensor-proxy-nabu]=iio-sensor-proxy-VERSION
release_value[iio-sensor-proxy-nabu]='1.nabu1.test%{?dist}'

is_known_package() {
    local candidate=$1 known
    for known in "${known_packages[@]}"; do
        [[ $candidate == "$known" ]] && return 0
    done
    return 1
}

latest_stable_version() {
    local repository=$1
    local prefix=$2
    git ls-remote --tags --refs "$repository" "refs/tags/${prefix}*" \
        | sed -n "s|.*refs/tags/${prefix}||p" \
        | grep -E '^[0-9]+\.[0-9]+(\.[0-9]+)?$' \
        | sort -V \
        | tail -n 1
}

work_dir=$(mktemp -d /tmp/nabu-userspace-update.XXXXXX)
trap 'rm -rf -- "$work_dir"' EXIT

for package in "${packages[@]}"; do
    is_known_package "$package" || {
        printf 'Unknown package: %s\n' "$package" >&2
        exit 2
    }

    package_dir="$repo_root/packaging/$package"
    spec="$package_dir/${package%-nabu}.spec"
    current=$(sed -n 's/^Version:[[:space:]]*//p' "$spec" | head -n 1)
    [[ -n $current ]] || {
        printf 'Cannot read Version from %s\n' "$spec" >&2
        exit 2
    }
    current_version[$package]=$current

    if [[ $mode == update ]]; then
        target=$(latest_stable_version "${upstream_repo[$package]}" "${tag_prefix[$package]}")
    else
        target=$current
    fi
    [[ -n $target ]] || {
        printf 'Cannot resolve an upstream version for %s\n' "$package" >&2
        exit 2
    }
    target_version[$package]=$target

    url=${source_url[$package]//VERSION/$target}
    filename=${archive_name[$package]//VERSION/$target}
    archive="$work_dir/$filename"
    curl -fsSL --retry 3 "$url" -o "$archive"
    target_sha[$package]=$(sha256sum "$archive" | awk '{print $1}')

    unpack="$work_dir/unpack-$package"
    mkdir -p "$unpack"
    tar -xzf "$archive" -C "$unpack"
    root=${source_root[$package]//VERSION/$target}
    source_dir="$unpack/$root"
    [[ -d $source_dir ]] || {
        printf 'Unexpected archive root for %s %s: %s\n' "$package" "$target" "$root" >&2
        exit 1
    }

    case "$package" in
        hexagonrpc-nabu)
            patches=(
                0001-serve-writable-files.patch
                0002-implement-fremove.patch
                0003-raise-listener-input-limit.patch
                0004-support-extended-frename.patch
				0005-keep-listener-alive-on-stale-requests.patch
            )
            ;;
		libssc-nabu)
			patches=(
				0001-libssc-avoid-use-after-free-in-sensor-error-logging.patch
				0002-libssc-accept-fractional-and-integer-mount-matrices.patch
				0003-ssccli-probe-arbitrary-data-types.patch
				0004-libssc-treat-zero-placement-as-unspecified.patch
				0005-libssc-add-TCS3701-CCT-sensor-support.patch
				0006-libssc-use-standard-measurement-id-for-CCT.patch
				0007-libssc-decode-packed-CCT-standard-event.patch
				0008-libssc-address-named-light-sensor-data-types.patch
				0009-ssccli-expose-LSM6DSO-temperature-stream.patch
			)
            ;;
        iio-sensor-proxy-nabu)
            patches=(
                0001-WIP-iio-sensor-proxy.c-Do-not-exit-based-on-sensor-e.patch
                0002-start-initial-sensors-claimed-during-discovery.patch
                0003-udev-standardize-Nabu-SDSP-orientation.patch
				0004-iio-sensor-proxy-avoid-SSC-I-O-after-hot-unplug.patch
				0005-iio-sensor-proxy-fuse-front-and-rear-SSC-light.patch
            )
            ;;
    esac

    for patch_name in "${patches[@]}"; do
        patch --batch --forward -d "$source_dir" -p1 \
            <"$package_dir/$patch_name" >/dev/null
    done

    if [[ $mode == check ]]; then
        expected="${target_sha[$package]}  $filename"
        grep -Fxq "$expected" "$package_dir/UPSTREAM.sha256" || {
            printf 'Upstream checksum lock mismatch for %s %s\n' "$package" "$target" >&2
            exit 1
        }
    fi

    printf '%s: upstream %s, Nabu merge patches apply cleanly\n' "$package" "$target"
done

if [[ $mode == update ]]; then
    for package in "${packages[@]}"; do
        current=${current_version[$package]}
        target=${target_version[$package]}
        [[ $current != "$target" ]] || continue

        package_dir="$repo_root/packaging/$package"
        spec="$package_dir/${package%-nabu}.spec"
        filename=${archive_name[$package]//VERSION/$target}
        sed -i \
            -e "0,/^Version:.*$/s//Version:        $target/" \
            -e "0,/^Release:.*$/s//Release:        ${release_value[$package]}/" \
            "$spec"
        printf '%s  %s\n' "${target_sha[$package]}" "$filename" \
            >"$package_dir/UPSTREAM.sha256"

        changelog=$(mktemp "$work_dir/changelog.XXXXXX")
        awk -v package="$package" -v version="$target" '
            { print }
            $0 == "%changelog" {
                print "* " strftime("%a %b %d %Y") " mcc45tr <mcc45tr@gmail.com> - " version "-1.nabu1.test"
                print "- Refresh from upstream and retain the verified SENEMOS/Nabu merge patch set."
                print "- Generated by packaging/update-upstreams.sh after patch compatibility checks."
                print ""
            }
        ' "$spec" >"$changelog"
        mv "$changelog" "$spec"
        printf '%s: prepared %s -> %s\n' "$package" "$current" "$target"
    done
fi
