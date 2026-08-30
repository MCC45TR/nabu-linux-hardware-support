SHELL := /bin/bash

.PHONY: srpm

srpm:
	set -euo pipefail; \
	repo_root="$$(cd -- "$$(dirname -- "$$(realpath "$(lastword $(MAKEFILE_LIST))")")/.." && pwd)"; \
	package_dir="$$(pwd)"; \
	case "$$(basename "$$package_dir")" in \
		libssc-nabu) package=libssc-nabu; spec_file=libssc.spec ;; \
		hexagonrpc-nabu) package=hexagonrpc-nabu; spec_file=hexagonrpc.spec ;; \
		*) printf 'Unsupported COPR package directory: %s\n' "$$package_dir" >&2; exit 2 ;; \
	esac; \
	if [[ -n "$(spec)" ]]; then spec_file="$(spec)"; fi; \
	version="$$(sed -n 's/^Version:[[:space:]]*//p' "$$spec_file" | head -n 1)"; \
	case "$$package" in \
		libssc-nabu) url="https://codeberg.org/DylanVanAssche/libssc/archive/v$$version.tar.gz"; archive="libssc-$$version.tar.gz" ;; \
		hexagonrpc-nabu) url="https://github.com/linux-msm/hexagonrpc/archive/refs/tags/v$$version.tar.gz"; archive="hexagonrpc-$$version.tar.gz" ;; \
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
