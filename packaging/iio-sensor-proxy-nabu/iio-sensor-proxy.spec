%global debug_package %{nil}
%global upstream_name iio-sensor-proxy

Name:           iio-sensor-proxy-nabu
Version:        3.9
Release:        111.nabu9.test%{?dist}
Summary:        Nabu IIO sensor service for orientation-aware desktops

# tests/unittest_inspector.py is LGPL-2.1-or-later but it is not packaged
License:        GPL-3.0-or-later
URL:            https://gitlab.freedesktop.org/hadess/iio-sensor-proxy
Source0:        %{url}/-/archive/%{version}/%{upstream_name}-%{version}.tar.gz

Patch0001:      0001-WIP-iio-sensor-proxy.c-Do-not-exit-based-on-sensor-e.patch
Patch0002:      0002-start-initial-sensors-claimed-during-discovery.patch
Patch0003:      0003-udev-standardize-Nabu-SDSP-orientation.patch

BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  git-core
BuildRequires:  gtk-doc
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(libssc)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(gudev-1.0)
BuildRequires:  pkgconfig(polkit-gobject-1)
BuildRequires:  systemd
BuildRequires:  systemd-udev
BuildRequires:  umockdev
BuildRequires:  python3-dbusmock
%{?systemd_requires}

Requires:       libssc-nabu
Requires:       dbus
Provides:       iio-sensor-proxy = %{version}-%{release}
Provides:       iio-sensor-proxy%{?_isa} = %{version}-%{release}
Obsoletes:      iio-sensor-proxy < %{version}-%{release}

%description
Nabu integration of iio-sensor-proxy for Fedora desktops on Xiaomi Pad 5. It
connects the Qualcomm sensor service to the standard IIO and D-Bus interfaces
used by orientation, automatic-brightness and adaptive user-session features.

%package -n iio-sensor-proxy-docs-nabu
Summary:        Documentation for %{name}
License:        GFDL-1.1-or-later
BuildArch:      noarch

%description -n iio-sensor-proxy-docs-nabu
Developer and administrator documentation for the Nabu iio-sensor-proxy
integration.

%prep
%autosetup -S git_am -n %{upstream_name}-%{version}

%build
%meson -Dgtk_doc=true -Dgtk-tests=false -Dssc-support=enabled
%meson_build

%install
%meson_install

%check
%meson_test

# Keep the Nabu SSC compass integration as an explicit release contract.
# Compass is exported on its dedicated standard D-Bus object rather than the
# main SensorProxy object.
grep -F 'ssc-compass' data/80-iio-sensor-proxy.rules
grep -F 'net.hadess.SensorProxy.Compass' data/net.hadess.SensorProxy.conf.in
grep -F 'HasCompass' tests/ssc-test.py
grep -F 'CompassHeading' tests/ssc-test.py
grep -F 'get_compass_dbus_property' tests/ssc-test.py

# Keep desktop orientation board-defined and desktop-agnostic. Nabu must use
# SDSP once, and iio-sensor-proxy must consume the standard udev mount matrix.
grep -F 'KERNEL=="fastrpc-adsp*", ENV{IIO_SENSOR_PROXY_TYPE}=""' \
    data/80-iio-sensor-proxy.rules
grep -F 'KERNEL=="fastrpc-sdsp*", ENV{IIO_SENSOR_PROXY_TYPE}="ssc-accel ssc-light ssc-compass"' \
    data/80-iio-sensor-proxy.rules
grep -F 'ENV{ACCEL_MOUNT_MATRIX}="$attr{mount_matrix}"' \
    data/80-iio-sensor-proxy.rules
udevadm verify data/80-iio-sensor-proxy.rules

%post
%systemd_post %{upstream_name}.service

%preun
%systemd_preun %{upstream_name}.service

%postun
%systemd_postun_with_restart %{upstream_name}.service

%posttrans
if [ -x /usr/bin/udevadm ]; then
    /usr/bin/udevadm control --reload >/dev/null 2>&1 || :
    /usr/bin/udevadm trigger --action=change \
        --subsystem-match=misc --sysname-match='fastrpc-*' >/dev/null 2>&1 || :
fi
if [ -x /usr/bin/systemctl ]; then
    case "$(/usr/bin/systemctl is-system-running 2>/dev/null || :)" in
        running|degraded)
            /usr/bin/systemctl try-restart %{upstream_name}.service >/dev/null 2>&1 || :
            ;;
    esac
fi

%files
%license COPYING
%doc README.md
%{_bindir}/monitor-sensor
%{_libexecdir}/%{upstream_name}
%{_unitdir}/%{upstream_name}.service
%{_udevrulesdir}/*-%{upstream_name}.rules
%{_datadir}/dbus-1/system.d/net.hadess.SensorProxy.conf
%{_datadir}/polkit-1/actions/net.hadess.SensorProxy.policy

%files -n iio-sensor-proxy-docs-nabu
%dir %{_datadir}/gtk-doc/
%dir %{_datadir}/gtk-doc/html/
%{_datadir}/gtk-doc/html/%{upstream_name}/

%changelog
* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 3.9-111.nabu9.test
- Require udevadm for the packaged rule validation gate

* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 3.9-110.nabu8.test
- Consolidate Nabu SSC ownership and mount-matrix rules into one contextual patch.
- Make the RPM git-am preparation path match the validated update path.

* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 3.9-109.nabu7.test
- Repair the ordered udev patch context after the SSC accelerometer patch.
- Keep Nabu SDSP as the sole SSC source and export the DT mount matrix.

* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 3.9-108.nabu6.test
- Select Nabu SDSP as the sole SSC desktop sensor source.
- Publish the kernel/Device Tree transform through ACCEL_MOUNT_MATRIX.
- Keep rotation correct across GNOME, KDE and other SensorProxy consumers.

* Mon Aug 31 2026 mcc45tr <mcc45tr@gmail.com> - 3.9-107.nabu5.test
- Move the proven Nabu SSC integration into the checksum-locked update pipeline.
- Retain hotplug survival, initial-claim polling and FastRPC accelerometer patches.
- Build from the stable upstream tag so future releases can be reviewed automatically.
