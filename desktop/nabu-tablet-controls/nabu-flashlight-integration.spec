Name:           nabu-flashlight-integration
Version:        1.0.0
Release:        15%{?dist}
Summary:        Xiaomi Pad 5 tablet controls for Plasma and GNOME
License:        GPL-3.0-or-later
URL:            https://copr.fedorainfracloud.org/coprs/mcc45tr/nabu-linux/
Source0:        %{name}-%{version}.tar.gz
Source1:        https://github.com/viveris/uMTP-Responder/archive/refs/tags/umtprd-1.8.1.tar.gz
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  python3
BuildRequires:  gettext
BuildRequires:  glib2
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  pkgconfig(Qt6DBus)
BuildRequires:  systemd-rpm-macros
Requires:       bluez
Requires:       feedbackd
Requires:       NetworkManager
Requires:       openssh-server
Requires:       polkit
Requires:       systemd

%description
A narrowly scoped native hardware controller plus stock Plasma 6 and GNOME
Shell integrations for the Xiaomi Pad 5. It uses kernel and desktop public
APIs, does not patch either desktop environment and runs no persistent service.

%package plasma
Summary:        Plasma 6 Tablet Control widget for Nabu
BuildArch:      noarch
Requires:       %{name} = %{version}-%{release}
Requires:       plasma-workspace
Requires:       plasma5support

%description plasma
Native Plasma 6 System Tray widget for Xiaomi Pad 5 tablet controls.

%package gnome
Summary:        GNOME Shell Quick Settings for Nabu tablet controls
BuildArch:      noarch
Requires:       %{name} = %{version}-%{release}
Requires:       gnome-shell

%description gnome
GNOME Shell Quick Settings extension for Xiaomi Pad 5 hardware controls.
It uses stock GNOME Shell and GNOME Settings Daemon APIs without replacing or
patching any GNOME package.

%prep
%autosetup -a 1

%build
%{__cc} %{build_cflags} %{build_ldflags} -o nabu-flashlight src/nabu-flashlight.c
%{__cc} %{build_cflags} %{build_ldflags} -o nabu-usb-role src/nabu-usb-role.c
%{__cxx} -std=c++17 %{build_cxxflags} $(pkg-config --cflags Qt6Core Qt6DBus) \
    -o nabu-accessory-state src/nabu-accessory-state.cpp \
    %{build_ldflags} $(pkg-config --libs Qt6Core Qt6DBus)
%{__make} -C uMTP-Responder-umtprd-1.8.1 \
    CC=%{__cc} \
    CFLAGS="%{build_cflags} -I./inc -Wall" \
    LDFLAGS="%{build_ldflags} -lpthread -lrt"
mkdir -p build-locale
mkdir -p build-gnome-locale
for po in translations/*.po; do
    lang="$(basename "$po" .po)"
    msgfmt --check --check-format -o "build-locale/$lang.mo" "$po"
    msgfmt --check --check-format -o "build-gnome-locale/$lang.mo" "$po"
done

%install
install -Dpm2755 nabu-flashlight %{buildroot}%{_libexecdir}/nabu-flashlight
install -Dpm0755 nabu-usb-role %{buildroot}%{_libexecdir}/nabu-usb-role
install -Dpm0755 nabu-accessory-state %{buildroot}%{_libexecdir}/nabu-accessory-state
install -Dpm0755 src/nabu-usb-gadget %{buildroot}%{_libexecdir}/nabu-usb-gadget
install -Dpm0755 src/nabu-umtprd-start %{buildroot}%{_libexecdir}/nabu-umtprd-start
install -Dpm0755 uMTP-Responder-umtprd-1.8.1/umtprd %{buildroot}%{_libexecdir}/nabu-umtprd
install -d %{buildroot}%{_bindir}
ln -s %{_libexecdir}/nabu-flashlight %{buildroot}%{_bindir}/nabu-flashlightctl
ln -s %{_libexecdir}/nabu-usb-role %{buildroot}%{_bindir}/nabu-usb-role
ln -s %{_libexecdir}/nabu-accessory-state %{buildroot}%{_bindir}/nabu-accessory-state
ln -s %{_libexecdir}/nabu-usb-gadget %{buildroot}%{_bindir}/nabu-usb-gadget
install -Dpm0644 systemd/nabu-usb-gadget.service \
    %{buildroot}%{_unitdir}/nabu-usb-gadget.service
install -Dpm0644 systemd/nabu-mtp-responder.service \
    %{buildroot}%{_unitdir}/nabu-mtp-responder.service
install -Dpm0644 systemd/nabu-adbd.service \
    %{buildroot}%{_unitdir}/nabu-adbd.service
install -Dpm0600 network/SENEMOS-USB-Gadget.nmconnection \
    %{buildroot}%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection
install -Dpm0644 polkit/org.senemos.nabu.tablet-control.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.tablet-control.policy
install -d %{buildroot}%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight
cp -a plasma/. %{buildroot}%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight/
for mo in build-locale/*.mo; do
    lang="$(basename "$mo" .mo)"
    install -Dpm0644 "$mo" \
        "%{buildroot}%{_datadir}/locale/$lang/LC_MESSAGES/plasma_applet_org.senemos.nabu.flashlight.mo"
done
install -Dpm0644 plasma-update/org.senemos.nabu.flashlight.js \
    %{buildroot}%{_datadir}/plasma/shells/org.kde.plasma.desktop/contents/updates/org.senemos.nabu.flashlight.js
install -d %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org
install -Dpm0644 gnome/extension.js \
    %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/extension.js
install -Dpm0644 gnome/metadata.json \
    %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/metadata.json
install -Dpm0644 gnome/prefs.js \
    %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/prefs.js
install -d %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/schemas
install -Dpm0644 gnome/schemas/*.gschema.xml \
    %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/schemas/
glib-compile-schemas --strict \
    %{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/schemas
for mo in build-gnome-locale/*.mo; do
    lang="$(basename "$mo" .mo)"
    install -Dpm0644 "$mo" \
        "%{buildroot}%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/locale/$lang/LC_MESSAGES/nabu_tablet_control.mo"
done
install -Dpm0755 gnome/integration/nabu-gnome-extension-enable \
    %{buildroot}%{_libexecdir}/nabu-gnome-extension-enable
install -Dpm0644 gnome/integration/nabu-gnome-extension-enable.service \
    %{buildroot}%{_userunitdir}/nabu-gnome-extension-enable.service
install -d %{buildroot}%{_userunitdir}/graphical-session.target.wants
ln -s ../nabu-gnome-extension-enable.service \
    %{buildroot}%{_userunitdir}/graphical-session.target.wants/nabu-gnome-extension-enable.service
%find_lang plasma_applet_org.senemos.nabu.flashlight

%check
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-flashlight)" = 2755
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-usb-role)" = 755
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-accessory-state)" = 755
test "$(stat -c '%%a' %{buildroot}%{_libexecdir}/nabu-usb-gadget)" = 755
test "$(stat -c '%%a' %{buildroot}%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection)" = 600
sh -n src/nabu-usb-gadget
sh -n src/nabu-umtprd-start
grep -Fq '/usr/libexec/nabu-usb-role' \
    %{buildroot}%{_datadir}/polkit-1/actions/org.senemos.nabu.tablet-control.policy
python3 -m json.tool plasma/metadata.json >/dev/null
python3 -m json.tool gnome/metadata.json >/dev/null
glib-compile-schemas --strict --dry-run gnome/schemas
sh -n gnome/integration/nabu-gnome-extension-enable
find translations -name '*.po' -exec msgfmt --check --check-format -o /dev/null {} \;
test "$(find translations -name '*.po' | wc -l)" = 27
! grep -REq 'repeat:[[:space:]]*true|refreshTimer|interval:[[:space:]]*(3000|15000)' plasma/contents/ui

%post
%systemd_post nabu-usb-gadget.service

%preun
%systemd_preun nabu-usb-gadget.service

%postun
%systemd_postun_with_restart nabu-usb-gadget.service

%files
%attr(2755,root,feedbackd) %{_libexecdir}/nabu-flashlight
%{_libexecdir}/nabu-usb-role
%{_libexecdir}/nabu-accessory-state
%{_libexecdir}/nabu-usb-gadget
%{_libexecdir}/nabu-umtprd-start
%{_libexecdir}/nabu-umtprd
%{_bindir}/nabu-flashlightctl
%{_bindir}/nabu-usb-role
%{_bindir}/nabu-accessory-state
%{_bindir}/nabu-usb-gadget
%{_unitdir}/nabu-usb-gadget.service
%{_unitdir}/nabu-mtp-responder.service
%{_unitdir}/nabu-adbd.service
%{_prefix}/lib/NetworkManager/system-connections/SENEMOS-USB-Gadget.nmconnection
%{_datadir}/polkit-1/actions/org.senemos.nabu.tablet-control.policy
%doc API.md
%license uMTP-Responder-umtprd-1.8.1/LICENSE

%files plasma -f plasma_applet_org.senemos.nabu.flashlight.lang
%{_datadir}/plasma/plasmoids/org.senemos.nabu.flashlight/
%{_datadir}/plasma/shells/org.kde.plasma.desktop/contents/updates/org.senemos.nabu.flashlight.js

%files gnome
%{_datadir}/gnome-shell/extensions/nabu-flashlight@senemos.org/
%{_libexecdir}/nabu-gnome-extension-enable
%{_userunitdir}/nabu-gnome-extension-enable.service
%{_userunitdir}/graphical-session.target.wants/nabu-gnome-extension-enable.service

%changelog
* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-15
- Use one-column native Quick Settings tiles and place sound below brightness.
- Apply flashlight levels on redraw, remember the last level while off and animate its slider.
- Add extension preferences, automatic USB power policy and connected-only accessory tiles.
- Rename the cover tile to Xiaomi Keyboard and correct its inverted presence state.

* Sat Aug 29 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-14
- Expand the stock GNOME Shell extension into capability-aware tablet controls.
- Add flashlight, ambient brightness, USB role and conditional accessory entries.
- Enable the extension once on the first GNOME login without overriding later user choice.

* Sat Aug 29 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-13
- Add conditional Smart Pen battery, charging and BlueZ connect controls.
- Show the pogo keyboard indicator only while the keyboard is attached.
- Ship the same 27-language catalog set as the File Search Plasma widget.

* Sat Aug 29 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-12
- Rebuild the Plasma widget as a scrollable Breeze-style Kirigami control panel.
- Replace polling with capability-aware event refresh and immediate optimistic state.
- Add live coalesced flashlight brightness and Turkish translations.

* Sat Aug 29 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-11
- Add a controllable MTP, USB-NCM/SSH and serial-console gadget profile.
- Stop the device gadget cleanly before requesting USB host mode.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-10
- Poll detailed tablet state only while the System Tray popup is open.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-9
- Present every Tablet Control state as a status row with an action button.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-8
- Declare the Plasma expanded-state signal argument explicitly.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-7
- Use an explicit Plasmoid property in the expanded-state handler.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-6
- Ignore KScreen ANSI formatting when reading display automation state.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-5
- Evolve the Plasma widget into Kirigami Tablet Control.
- Publish the safe flashlight CLI and add polkit-gated USB-C role requests.

* Fri Aug 28 2026 Senen OS <support@senemos.com> - 1.0.0-4
- Use the Plasma 6 scripting API for System Tray registration
- Mark the plasmoid as tray-loadable and avoid an inherited QML property clash

* Fri Aug 28 2026 SENEMOS Nabu <nabu@senemos.org> - 1.0.0-3
- Add hardware-limited brightness controls and coordinated dual-LED support.

* Fri Aug 28 2026 SENEMOS Nabu <nabu@senemos.org> - 1.0.0-2
- Refresh the Plasma indicator when another client changes the LED.

* Fri Aug 28 2026 SENEMOS Nabu <nabu@senemos.org> - 1.0.0-1
- Add service-free Plasma and GNOME flashlight controls.
