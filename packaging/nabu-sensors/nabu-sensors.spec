%global debug_package %{nil}

Name:           nabu-sensors
Version:        2026.9.6
Release:        1%{?dist}
Summary:        Unified Qualcomm sensor stack for Xiaomi Pad 5
License:        GPL-3.0-or-later AND GFDL-1.1-or-later
URL:            https://github.com/MCC45TR/nabu-linux-hardware-support
ExclusiveArch:  aarch64

Source0:        libssc-0.4.4.tar.gz
Source1:        iio-sensor-proxy-3.9.tar.gz
Source2:        nabu-sensors-patches.sha256
Patch0001:      libssc-0001-libssc-avoid-use-after-free-in-sensor-error-logging.patch
Patch0002:      libssc-0002-libssc-accept-fractional-and-integer-mount-matrices.patch
Patch0003:      libssc-0003-ssccli-probe-arbitrary-data-types.patch
Patch0004:      libssc-0004-libssc-treat-zero-placement-as-unspecified.patch
Patch0005:      libssc-0005-libssc-add-TCS3701-CCT-sensor-support.patch
Patch0006:      libssc-0006-libssc-use-standard-measurement-id-for-CCT.patch
Patch0007:      libssc-0007-libssc-decode-packed-CCT-standard-event.patch
Patch0008:      libssc-0008-libssc-address-named-light-sensor-data-types.patch
Patch0009:      libssc-0009-ssccli-expose-LSM6DSO-temperature-stream.patch
Patch0010:      libssc-0010-ssccli-expose-LSM6DSO-motion-detect-events.patch
Patch0011:      libssc-0011-libssc-support-single-output-sensor-streams.patch
Patch0101:      iio-0001-WIP-iio-sensor-proxy.c-Do-not-exit-based-on-sensor-e.patch
Patch0102:      iio-0002-start-initial-sensors-claimed-during-discovery.patch
Patch0103:      iio-0003-udev-standardize-Nabu-SDSP-orientation.patch
Patch0104:      iio-0004-iio-sensor-proxy-avoid-SSC-I-O-after-hot-unplug.patch
Patch0105:      iio-0005-iio-sensor-proxy-fuse-front-and-rear-SSC-light.patch

BuildRequires:  gcc
BuildRequires:  git-core
BuildRequires:  gtk-doc
BuildRequires:  meson
BuildRequires:  python3-dbusmock
BuildRequires:  python3-devel
BuildRequires:  python3-gobject-base
BuildRequires:  python3-protobuf
BuildRequires:  systemd
BuildRequires:  systemd-udev
BuildRequires:  umockdev
BuildRequires:  /usr/bin/protoc
BuildRequires:  /usr/bin/protoc-c
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gudev-1.0)
BuildRequires:  pkgconfig(libprotobuf-c)
BuildRequires:  pkgconfig(polkit-gobject-1)
BuildRequires:  pkgconfig(qmi-glib)
BuildRequires:  pkgconfig(qrtr)
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(udev)

%description
One checksum-locked source package for the Nabu Qualcomm Sensor Core library,
Python bindings and standard SensorProxy service. Binary package names remain
stable so existing installations move to this consolidated source via DNF.

%package -n libssc-nabu
Summary:        Qualcomm Sensor Core client library for Nabu
Provides:       libssc = 0.4.4
Obsoletes:      libssc < 0.4.4

%description -n libssc-nabu
Qualcomm Sensor Core client library and diagnostic CLI for Xiaomi Pad 5.

%package -n libssc-nabu-devel
Summary:        Development headers for libssc
Requires:       libssc-nabu%{?_isa} = %{version}-%{release}
Provides:       libssc-devel = 0.4.4
Obsoletes:      libssc-devel < 0.4.4

%description -n libssc-nabu-devel
Development headers and pkg-config metadata for the Nabu SSC client library.

%package -n python3-ssc-nabu
Summary:        Python bindings and mock server for libssc
Requires:       libssc-nabu%{?_isa} = %{version}-%{release}
Requires:       python3-gobject-base
Requires:       python3-protobuf
Provides:       python3-ssc = 0.4.4
Obsoletes:      python3-ssc < 0.4.4

%description -n python3-ssc-nabu
Python bindings and installed-test mock server for the Nabu SSC library.

%package -n iio-sensor-proxy-nabu
Summary:        Nabu IIO sensor service for orientation-aware desktops
Requires:       libssc-nabu%{?_isa} = %{version}-%{release}
Requires:       dbus
Provides:       iio-sensor-proxy = 3.9
Provides:       iio-sensor-proxy%{?_isa} = 3.9
Obsoletes:      iio-sensor-proxy < 3.9

%description -n iio-sensor-proxy-nabu
Nabu integration of iio-sensor-proxy for standard orientation, compass and
automatic-brightness interfaces.

%package -n iio-sensor-proxy-docs-nabu
Summary:        Documentation for iio-sensor-proxy-nabu
License:        GFDL-1.1-or-later
BuildArch:      noarch

%description -n iio-sensor-proxy-docs-nabu
Developer and administrator documentation for the Nabu SensorProxy service.

%prep
%setup -q -c -T
(cd %{_sourcedir} && sha256sum -c %{SOURCE2})
tar -xf %{SOURCE0}
tar -xf %{SOURCE1}
pushd libssc
%patch -P 1 -p1
%patch -P 2 -p1
%patch -P 3 -p1
%patch -P 4 -p1
%patch -P 5 -p1
%patch -P 6 -p1
%patch -P 7 -p1
%patch -P 8 -p1
%patch -P 9 -p1
%patch -P 10 -p1
%patch -P 11 -p1
popd
pushd iio-sensor-proxy-3.9
git init -q
git config user.name rpm-build
git config user.email rpm-build@localhost
git add -A
git commit -qm 'Upstream baseline'
git am -q %{PATCH101} %{PATCH102} %{PATCH103} %{PATCH104} %{PATCH105}
popd

%build
common_options=(
    --buildtype=plain
    --prefix=%{_prefix}
    --bindir=%{_bindir}
    --libdir=%{_libdir}
    --libexecdir=%{_libexecdir}
    --includedir=%{_includedir}
    --datadir=%{_datadir}
    --sysconfdir=%{_sysconfdir}
    --localstatedir=%{_localstatedir}
    --mandir=%{_mandir}
)
pushd libssc
CFLAGS='%{optflags}' LDFLAGS='%{build_ldflags}' \
    meson setup build "${common_options[@]}"
meson compile -C build %{?_smp_mflags}
DESTDIR="$PWD/../libssc-stage" meson install -C build
popd

# Expose the just-built libssc to the second Meson project without applying a
# global pkg-config sysroot.  A global sysroot also rewrites native GLib tools
# such as glib-compile-resources into the staging tree.
install -d libssc-pkgconfig
cp libssc-stage%{_libdir}/pkgconfig/libssc.pc libssc-pkgconfig/
sed -i "s|^prefix=.*|prefix=$PWD/libssc-stage%{_prefix}|" \
    libssc-pkgconfig/libssc.pc

pushd iio-sensor-proxy-3.9
PKG_CONFIG_PATH="$PWD/../libssc-pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
CFLAGS='%{optflags}' LDFLAGS='%{build_ldflags}' \
    meson setup build "${common_options[@]}" \
        -Dgtk_doc=true -Dgtk-tests=false -Dssc-support=enabled
meson compile -C build %{?_smp_mflags}
popd

%install
pushd libssc
DESTDIR=%{buildroot} meson install -C build
popd
pushd iio-sensor-proxy-3.9
DESTDIR=%{buildroot} meson install -C build
popd

install -d %{buildroot}%{_unitdir}/iio-sensor-proxy.service.d
cat >%{buildroot}%{_unitdir}/iio-sensor-proxy.service.d/30-nabu-bounded-stop.conf <<'EOF'
[Service]
TimeoutStopSec=5s
TimeoutStopFailureMode=terminate
EOF

%check
meson test -C libssc/build --print-errorlogs
meson test -C iio-sensor-proxy-3.9/build --print-errorlogs
grep -F 'SSC_STREAM_TYPE_SINGLE_OUTPUT' libssc/src/libssc-sensor.c
grep -F '"ambient_light_back"' iio-sensor-proxy-3.9/src/drv-ssc-light.c
grep -F 'MIN (rear->intensity, drv_data->published)' iio-sensor-proxy-3.9/src/drv-ssc-light.c
udevadm verify iio-sensor-proxy-3.9/data/80-iio-sensor-proxy.rules

%post -n iio-sensor-proxy-nabu
%systemd_post iio-sensor-proxy.service

%preun -n iio-sensor-proxy-nabu
%systemd_preun iio-sensor-proxy.service

%postun -n iio-sensor-proxy-nabu
%systemd_postun_with_restart iio-sensor-proxy.service

%posttrans -n iio-sensor-proxy-nabu
if [ -x /usr/bin/udevadm ]; then
    /usr/bin/udevadm control --reload >/dev/null 2>&1 || :
    /usr/bin/udevadm trigger --action=change \
        --subsystem-match=misc --sysname-match='fastrpc-*' >/dev/null 2>&1 || :
fi

%files -n libssc-nabu
%license libssc/LICENSE
%{_bindir}/ssccli
%{_libdir}/libssc.so.2

%files -n libssc-nabu-devel
%{_includedir}/libssc
%{_libdir}/libssc.so
%{_libdir}/pkgconfig/libssc.pc

%files -n python3-ssc-nabu
%{python3_sitelib}/ssc_server/
%dir %{_libexecdir}/installed-tests
%dir %{_libexecdir}/installed-tests/libssc
%{_libexecdir}/installed-tests/libssc/ssc-server

%files -n iio-sensor-proxy-nabu
%license iio-sensor-proxy-3.9/COPYING
%doc iio-sensor-proxy-3.9/README.md
%{_bindir}/monitor-sensor
%{_libexecdir}/iio-sensor-proxy
%{_unitdir}/iio-sensor-proxy.service
%dir %{_unitdir}/iio-sensor-proxy.service.d
%{_unitdir}/iio-sensor-proxy.service.d/30-nabu-bounded-stop.conf
%{_udevrulesdir}/*-iio-sensor-proxy.rules
%{_datadir}/dbus-1/system.d/net.hadess.SensorProxy.conf
%{_datadir}/polkit-1/actions/net.hadess.SensorProxy.policy

%files -n iio-sensor-proxy-docs-nabu
%dir %{_datadir}/gtk-doc/
%dir %{_datadir}/gtk-doc/html/
%{_datadir}/gtk-doc/html/iio-sensor-proxy/

%changelog
* Sun Sep 06 2026 mcc45tr <mcc45tr@gmail.com> - 2026.9.6-1
- Consolidate libssc, Python SSC bindings and SensorProxy into one COPR source.
- Preserve all existing binary package names for transaction-safe DNF updates.
