Name:           nabu-hardware-provenance
Version:        1.0.0
Release:        4%{?dist}
Summary:        Privacy-preserving hardware provenance for Xiaomi Pad 5
License:        MIT
URL:            https://github.com/MCC45TR/nabu-linux-hardware-support
Source0:        %{name}-%{version}.tar.gz
Source1:        nabu-hardware-provenance.sysusers
BuildRequires:  gcc-c++
BuildRequires:  python3
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  systemd-rpm-macros
BuildRequires:  systemd-udev
Requires:       systemd-udev
Recommends:     xiaomi-nabu-firmware

%description
Generate a bounded, read-only inventory of Xiaomi Pad 5 hardware data sources.
Android DTBO images are structurally inspected without applying them.  Other
Android firmware and calibration partitions remain opaque, while packaged
Linux firmware and camera NVMEM provider metadata are reported without exposing
device identities, addresses, calibration bytes or EEPROM contents.

%prep
%autosetup

%build
%{__cxx} -std=c++20 %{optflags} %{build_ldflags} \
    $(pkg-config --cflags Qt6Core) nabu-hardware-provenance.cpp \
    -o nabu-hardware-provenance $(pkg-config --libs Qt6Core)

%install
install -Dpm0755 nabu-hardware-provenance \
    %{buildroot}%{_libexecdir}/senemos-nabu/nabu-hardware-provenance
install -Dpm0644 nabu-hardware-provenance.service \
    %{buildroot}%{_unitdir}/nabu-hardware-provenance.service
install -Dpm0644 60-nabu-hardware-provenance.preset \
    %{buildroot}%{_presetdir}/60-nabu-hardware-provenance.preset
install -Dpm0644 %{SOURCE1} \
    %{buildroot}%{_sysusersdir}/nabu-hardware-provenance.conf
install -Dpm0644 70-nabu-hardware-provenance.rules \
    %{buildroot}%{_udevrulesdir}/70-nabu-hardware-provenance.rules

%check
NABU_PROVENANCE_BINARY="$PWD/nabu-hardware-provenance" \
    python3 -m unittest discover -s tests -v
test "$(od -An -tx1 -N4 nabu-hardware-provenance | tr -d ' \n')" = 7f454c46
grep -F 'DevicePolicy=closed' nabu-hardware-provenance.service
grep -F 'DeviceAllow=block-sd r' nabu-hardware-provenance.service
grep -F 'DeviceAllow=block-blkext r' nabu-hardware-provenance.service
grep -F 'CapabilityBoundingSet=' nabu-hardware-provenance.service
grep -F 'User=nabu-provenance' nabu-hardware-provenance.service
! grep -F 'RemainAfterExit=yes' nabu-hardware-provenance.service
udevadm verify --resolve-names=late 70-nabu-hardware-provenance.rules
test "$(grep -Fc 'ENV{SYSTEMD_WANTS}+="nabu-hardware-provenance.service"' \
    70-nabu-hardware-provenance.rules)" -eq 2
grep -F 'SUBSYSTEM=="net", KERNEL=="wld0"' 70-nabu-hardware-provenance.rules

%pre
%sysusers_create_package nabu-hardware-provenance %{SOURCE1}

%post
%systemd_post nabu-hardware-provenance.service
if [ -x /usr/bin/udevadm ]; then
    /usr/bin/udevadm control --reload >/dev/null 2>&1 || :
    for label in dtbo_a dtbo_b; do
        /usr/bin/udevadm trigger --action=change --subsystem-match=block \
            --property-match="ID_PART_ENTRY_NAME=$label" >/dev/null 2>&1 || :
    done
    /usr/bin/udevadm trigger --action=change --subsystem-match=net \
        --sysname-match=wld0 >/dev/null 2>&1 || :
fi

%preun
%systemd_preun nabu-hardware-provenance.service

%postun
%systemd_postun_with_restart nabu-hardware-provenance.service

%files
%license LICENSE
%doc README.md
%{_libexecdir}/senemos-nabu/nabu-hardware-provenance
%{_unitdir}/nabu-hardware-provenance.service
%{_presetdir}/60-nabu-hardware-provenance.preset
%{_sysusersdir}/nabu-hardware-provenance.conf
%{_udevrulesdir}/70-nabu-hardware-provenance.rules

%changelog
* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-4
- Re-run the bounded C++ inventory when wld0 appears so late radio metadata is
  present without a daemon, polling loop, network access, or address export.

* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-3
- Replace the production Python inventory with a hardened C++20/QtCore binary;
  retain Python only for isolated black-box package tests.

* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-2
- Trigger the bounded read-only inventory when the complete DTBO A/B pair is
  enumerated, while retaining the Linux-only multi-user fallback.
- Admit Nabu's blkext partition major read-only beneath the per-node DAC gate.

* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-1
- Add fail-closed Android DTBO A/B inventory and explicit slot confidence.
- Report packaged DSP/radio firmware and camera NVMEM metadata without secrets.
- Enforce read-only block access and a hardened, networkless system service.
