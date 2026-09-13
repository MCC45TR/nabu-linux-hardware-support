Name:           nabu-hardware-provenance
Version:        1.0.0
Release:        1%{?dist}
Summary:        Privacy-preserving hardware provenance for Xiaomi Pad 5
License:        MIT
URL:            https://github.com/MCC45TR/nabu-linux-hardware-support
Source0:        %{name}-%{version}.tar.gz
Source1:        nabu-hardware-provenance.sysusers
BuildArch:      noarch

BuildRequires:  python3
BuildRequires:  systemd-rpm-macros
BuildRequires:  systemd-udev
Requires:       python3
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
python3 -m unittest discover -s tests -v
python3 -m py_compile nabu-hardware-provenance
grep -F 'DevicePolicy=closed' nabu-hardware-provenance.service
grep -F 'DeviceAllow=block-sd r' nabu-hardware-provenance.service
grep -F 'DeviceAllow=block-blkext r' nabu-hardware-provenance.service
grep -F 'CapabilityBoundingSet=' nabu-hardware-provenance.service
grep -F 'User=nabu-provenance' nabu-hardware-provenance.service
udevadm verify --resolve-names=late 70-nabu-hardware-provenance.rules

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
* Sun Sep 13 2026 mcc45tr <mcc45tr@gmail.com> - 1.0.0-1
- Add fail-closed Android DTBO A/B inventory and explicit slot confidence.
- Report packaged DSP/radio firmware and camera NVMEM metadata without secrets.
- Enforce read-only block access and a hardened, networkless system service.
