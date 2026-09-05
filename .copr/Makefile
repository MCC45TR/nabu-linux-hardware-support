SHELL := /bin/bash

.PHONY: srpm

srpm:
	set -euo pipefail; \
	repo_root="$$(cd -- "$$(dirname -- "$$(realpath "$(lastword $(MAKEFILE_LIST))")")/.." && pwd)"; \
	package_dir="$$(pwd)"; \
	case "$$(basename "$$package_dir")" in \
		libssc-nabu) package=libssc-nabu; spec_file=libssc.spec ;; \
		hexagonrpc-nabu) package=hexagonrpc-nabu; spec_file=hexagonrpc.spec ;; \
		iio-sensor-proxy-nabu) package=iio-sensor-proxy-nabu; spec_file=iio-sensor-proxy.spec ;; \
		xiaomi-nabu-firmware) package=xiaomi-nabu-firmware; spec_file=xiaomi-nabu-firmware.spec ;; \
		*) printf 'Unsupported COPR package directory: %s\n' "$$package_dir" >&2; exit 2 ;; \
	esac; \
	if [[ -n "$(spec)" ]]; then spec_file="$(spec)"; fi; \
	version="$$(sed -n 's/^Version:[[:space:]]*//p' "$$spec_file" | head -n 1)"; \
	case "$$package" in \
		libssc-nabu) url="https://codeberg.org/DylanVanAssche/libssc/archive/v$$version.tar.gz"; archive="libssc-$$version.tar.gz" ;; \
		hexagonrpc-nabu) url="https://github.com/linux-msm/hexagonrpc/archive/refs/tags/v$$version.tar.gz"; archive="hexagonrpc-$$version.tar.gz" ;; \
		iio-sensor-proxy-nabu) url="https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/-/archive/$$version/iio-sensor-proxy-$$version.tar.gz"; archive="iio-sensor-proxy-$$version.tar.gz" ;; \
		xiaomi-nabu-firmware) url="https://gitlab.postmarketos.org/panpanpanpan/nabu-firmware/-/archive/$$version/nabu-firmware-$$version.tar.gz"; archive="nabu-firmware-$$version.tar.gz" ;; \
	esac; \
	command -v curl >/dev/null || dnf install -y curl; \
	command -v rpmbuild >/dev/null || dnf install -y rpm-build; \
	curl -fsSL --retry 3 "$$url" -o "$$archive"; \
	sha256sum -c UPSTREAM.sha256; \
	"$$repo_root/packaging/update-upstreams.sh" --check; \
	mkdir -p "$(outdir)"; \
	rpmbuild -bs "$$spec_file" \
		--define "_sourcedir $$package_dir" \
		--define "_srcrpmdir $(outdir)"
