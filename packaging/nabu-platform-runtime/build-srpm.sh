#!/usr/bin/bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
top=${1:-$root/rpmbuild}
mkdir -p "$top"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

curl -fsSL --retry 3 \
    https://github.com/linux-msm/hexagonrpc/archive/refs/tags/v0.5.0.tar.gz \
    -o "$top/SOURCES/hexagonrpc-0.5.0.tar.gz"
curl -fsSL --retry 3 \
    https://github.com/MCC45TR/iris-vaapi/archive/9be612d394a4622f9393ab9645698fe40f818c02.tar.gz \
    -o "$top/SOURCES/iris-vaapi-9be612d394a4622f9393ab9645698fe40f818c02.tar.gz"

install -m0644 "$root"/{hexagonrpcd-adsp-rootpd.service,hexagonrpcd-adsp-sensorspd.service,hexagonrpcd-sdsp.service,sysusers.conf,10-fastrpc.rules} "$top/SOURCES/"
install -m0644 "$root"/*.patch "$top/SOURCES/"
install -m0644 "$root/UPSTREAM.sha256" "$root/SOURCES.sha256" "$top/SOURCES/"
(
    cd "$top/SOURCES"
    sha256sum -c UPSTREAM.sha256
    sha256sum -c SOURCES.sha256
)
install -m0644 "$root/hexagonrpc.spec" "$top/SPECS/"
rpmbuild -bs --define "_topdir $top" "$top/SPECS/hexagonrpc.spec"
