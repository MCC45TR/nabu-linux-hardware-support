Name:           plasma-nabu-kcm
Version:        1.0.0
Release:        4%{?dist}
Summary:        Native Xiaomi Pad 5 settings module and Plasma widget
License:        MIT AND GPL-3.0-or-later
URL:            https://github.com/MCC45TR/nabu-linux-hardware-support
Source0:        %{name}-%{version}.tar.gz
Source1:        https://github.com/viveris/uMTP-Responder/archive/refs/tags/umtprd-1.8.1.tar.gz

ExclusiveArch:  aarch64
BuildRequires:  cmake >= 3.28
BuildRequires:  extra-cmake-modules >= 6.0
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  gcc
BuildRequires:  ninja-build
BuildRequires:  make
BuildRequires:  python3
BuildRequires:  systemd-rpm-macros
BuildRequires:  cmake(KF6CoreAddons)
BuildRequires:  cmake(KF6I18n)
BuildRequires:  cmake(KF6KCMUtils)
BuildRequires:  pkgconfig(Qt6Core) >= 6.8
BuildRequires:  pkgconfig(Qt6DBus) >= 6.8
BuildRequires:  pkgconfig(Qt6Gui) >= 6.8
BuildRequires:  pkgconfig(Qt6Qml) >= 6.8
BuildRequires:  pkgconfig(Qt6Quick) >= 6.8
Requires:       plasma-systemsettings
Requires:       plasma-workspace
Requires:       plasma5support
Requires:       NetworkManager
Requires:       openssh-server
Requires:       polkit
Requires:       systemd
Requires:       /usr/bin/nabu-accessory-state
Requires:       /usr/bin/nabu-flashlightctl
Requires:       /usr/bin/nabu-usb-role
Requires:       /usr/libexec/nabu-sar-control
Provides:       nabu-flashlight-integration-plasma = 3.0.0-107
Obsoletes:      nabu-flashlight-integration-plasma < 9999999999-99

%description
A Qt 6/KF 6 System Settings module and Plasma 6 System Tray widget for the
Xiaomi Pad 5 (nabu). Both front ends use the same narrowly scoped C++20 wake
controller. Sensor diagnostics are read-only and privileged changes are
allowlisted through polkit.

%prep
%autosetup -a 1

%build
%{__cxx} -std=c++20 %{build_cxxflags} $(pkg-config --cflags Qt6Core Qt6DBus) \
    -o nabu-wake-control src/nabu-wake-control.cpp \
    %{build_ldflags} $(pkg-config --libs Qt6Core Qt6DBus)
%{__cxx} -std=c++20 %{build_cxxflags} $(pkg-config --cflags Qt6Core Qt6DBus) \
    -o nabu-sar-calibration src/nabu-sar-calibration.cpp \
    %{build_ldflags} $(pkg-config --libs Qt6Core Qt6DBus)
%{__make} -C uMTP-Responder-umtprd-1.8.1 \
    CC=%{__cc} \
    CFLAGS="%{build_cflags} -I./inc -Wall" \
    LDFLAGS="%{build_ldflags} -lpthread -lrt"
%{_qt6_libexecdir}/moc src/nabu-wake-service.cpp -o nabu-wake-service.moc
%{__cxx} -std=c++20 %{build_cxxflags} -I. $(pkg-config --cflags Qt6Core Qt6DBus) \
    -o nabu-wake-service src/nabu-wake-service.cpp \
    %{build_ldflags} $(pkg-config --libs Qt6Core Qt6DBus)
%{__cmake} -S kcm -B build-kcm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DKDE_INSTALL_LIBDIR=%{_lib}
%{__cmake} --build build-kcm --parallel %{?_smp_build_ncpus}

mkdir -p build-locale
for po in translations/*.po; do
    lang="$(basename "$po" .po)"
    msgfmt --check --check-format -o "build-locale/$lang.mo" "$po"
done

%install
install -Dpm0755 nabu-wake-control %{buildroot}%{_libexecdir}/nabu-wake-control
install -Dpm0755 nabu-sar-calibration %{buildroot}%{_libexecdir}/nabu-sar-calibration
install -Dpm0755 src/nabu-usb-gadget %{buildroot}%{_libexecdir}/nabu-usb-gadget
install -Dpm0755 src/nabu-umtprd-start %{buildroot}%{_libexecdir}/nabu-umtprd-start
install -Dpm0755 uMTP-Responder-umtprd-1.8.1/umtprd %{buildroot}%{_libexecdir}/nabu-umtprd
install -d %{buildroot}%{_bindir}
ln -s %{_libexecdir}/nabu-usb-gadget %{buildroot}%{_bindir}/nabu-usb-gadget
install -Dpm0755 nabu-wake-service %{buildroot}%{_libexecdir}/nabu-wake-service
install -Dpm0644 systemd/nabu-wake-service.service \
    %{buildroot}%{_unitdir}/nabu-wake-service.service
install -Dpm0644 systemd/nabu-wake-apply.service \
    %{buildroot}%{_unitdir}/nabu-wake-apply.service
install -Dpm0644 systemd/nabu-usb-gadget.service \
    %{buildroot}%{_unitdir}/nabu-usb-gadget.service
install -Dpm0644 systemd/nabu-mtp-responder.service \
    %{buildroot}%{_unitdir}/nabu-mtp-responder.service
install -Dpm0644 systemd/nabu-adbd.service \
    %{buildroot}%{_unitdir}/nabu-adbd.service
install -Dpm0644 systemd/90-nabu-wake.preset \
    %{buildroot}%{_presetdir}/90-nabu-wake.preset
install -Dpm0644 tmpfiles/nabu-wake.conf \
    %{buildroot}%{_tmpfilesdir}/nabu-wake.conf
install -Dpm0644 dbus/org.senemos.Nabu.Wake.conf \
    %{buildroot}%{_datadir}/dbus-1/system.d/org.senemos.Nabu.Wake.conf
install -Dpm0644 polkit/org.senemos.nabu.wake-control.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.wake-control.policy
install -Dpm0644 polkit/org.senemos.nabu.sar-calibration.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.sar-calibration.policy
install -Dpm0600 network/SENEMOS-USB-Gadget.nmconnection \
    %{buildroot}%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection

install -d %{buildroot}%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight
cp -a plasma/. %{buildroot}%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight/
install -Dpm0644 plasma-update/org.senemos.nabu.flashlight.js \
    %{buildroot}%{_datadir}/plasma/shells/org.kde.plasma.desktop/contents/updates/org.senemos.nabu.flashlight.js
for mo in build-locale/*.mo; do
    lang="$(basename "$mo" .mo)"
    install -Dpm0644 "$mo" \
        "%{buildroot}%{_datadir}/locale/$lang/LC_MESSAGES/plasma_applet_org.senemos.nabu.flashlight.mo"
done
DESTDIR=%{buildroot} %{__cmake} --install build-kcm
%find_lang plasma_applet_org.senemos.nabu.flashlight

%check
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-wake-control)" = 755
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-sar-calibration)" = 755
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-usb-gadget)" = 755
test "$(stat -c '%%a' %{buildroot}%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection)" = 600
sh -n src/nabu-usb-gadget
sh -n src/nabu-umtprd-start
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-wake-service)" = 755
python3 tests/test-wake-control.py ./nabu-wake-control src/nabu-wake-control.cpp
python3 tests/test-sar-calibration.py ./nabu-sar-calibration src/nabu-sar-calibration.cpp
python3 tests/test-kcm-contract.py
python3 -m json.tool plasma/metadata.json >/dev/null
grep -Fq 'Double tap to wake' plasma/contents/ui/main.qml
grep -Fq 'Tilt to wake' plasma/contents/ui/main.qml
grep -Fq 'source: "configInfo.qml"' plasma/contents/config/config.qml
! grep -Fq 'title: i18n("Motion and gesture algorithms")' plasma/contents/ui/main.qml
test -f %{buildroot}%{_qt6_plugindir}/plasma/kcms/systemsettings/kcm_nabu.so
test -f %{buildroot}%{_datadir}/applications/kcm_nabu.desktop
grep -aFq 'X-KDE-System-Settings-Parent-Category' \
    %{buildroot}%{_qt6_plugindir}/plasma/kcms/systemsettings/kcm_nabu.so
grep -Fq '/usr/libexec/nabu-wake-control' \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.wake-control.policy
grep -Fq '/usr/libexec/nabu-sar-calibration' \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.sar-calibration.policy
grep -Fq 'ConditionPathExists=/dev/uinput' \
    %{buildroot}%{_unitdir}/nabu-wake-service.service
! grep -RIl '^#!.*python' %{buildroot}%{_libexecdir}/nabu-wake-* >/dev/null
find translations -name '*.po' -exec msgfmt --check --check-format -o /dev/null {} \;
test "$(find translations -name '*.po' | wc -l)" = 27
! grep -REq 'repeat:[[:space:]]*true|refreshTimer|interval:[[:space:]]*(3000|15000)' plasma/contents/ui

%post
%systemd_post nabu-wake-service.service nabu-wake-apply.service nabu-usb-gadget.service
%tmpfiles_create %{_tmpfilesdir}/nabu-wake.conf

%preun
%systemd_preun nabu-wake-service.service nabu-wake-apply.service nabu-usb-gadget.service

%postun
%systemd_postun_with_restart nabu-wake-service.service nabu-wake-apply.service nabu-usb-gadget.service

%files -f plasma_applet_org.senemos.nabu.flashlight.lang
%license LICENSE
%doc API.md
%{_libexecdir}/nabu-wake-control
%{_libexecdir}/nabu-sar-calibration
%{_libexecdir}/nabu-usb-gadget
%{_libexecdir}/nabu-umtprd-start
%{_libexecdir}/nabu-umtprd
%{_bindir}/nabu-usb-gadget
%{_libexecdir}/nabu-wake-service
%{_unitdir}/nabu-wake-service.service
%{_unitdir}/nabu-wake-apply.service
%{_unitdir}/nabu-usb-gadget.service
%{_unitdir}/nabu-mtp-responder.service
%{_unitdir}/nabu-adbd.service
%{_presetdir}/90-nabu-wake.preset
%{_tmpfilesdir}/nabu-wake.conf
%{_datadir}/dbus-1/system.d/org.senemos.Nabu.Wake.conf
%{_datadir}/polkit-1/actions/org.senemos.nabu.wake-control.policy
%{_datadir}/polkit-1/actions/org.senemos.nabu.sar-calibration.policy
%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection
%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight/
%{_datadir}/plasma/shells/org.kde.plasma.desktop/contents/updates/org.senemos.nabu.flashlight.js
%{_qt6_plugindir}/plasma/kcms/systemsettings/kcm_nabu.so
%{_datadir}/applications/kcm_nabu.desktop
%{_datadir}/applications/org.senemos.nabu.kcm.desktop
%license uMTP-Responder-umtprd-1.8.1/LICENSE

%changelog
* Mon Sep 14 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-4
- Include the reviewed spec in Source0 for the packaged contract test.

* Mon Sep 14 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-3
- Resolve package-test helper paths before subprocess execution in mock.

* Mon Sep 14 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-2
- Replace the legacy standalone Plasma integration without file conflicts.

* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-1
- Add a native Qt 6/KF 6 System Settings module for Nabu tablet controls.
- Merge the Plasma widget and move sensor explanations to its Information page.
- Add allowlisted double-tap and opt-in Sensor DSP tilt wake controls in C++20.
- Add event-driven live telemetry, safe Linux-side SAR calibration, and complete
  flashlight, USB-C, pen, keyboard, display, cover, wake and grip KCM controls.
