Name:           xiaomi-nabu-firmware
Version:        1
Release:        3.nabu1%{?dist}
Summary:        Firmware for Xiaomi Pad 5 (nabu)
URL:            https://gitlab.postmarketos.org/panpanpanpan/nabu-firmware
Source0:        %{url}/-/archive/%{version}/nabu-firmware-%{version}.tar.gz
BuildArch:      noarch
Requires:       qcom-firmware
License:        LicenseRef-Proprietary

%global _firmwaredir %{_prefix}/lib/firmware

%description
Device firmware required by the Xiaomi Pad 5 (nabu) hardware integration.

%prep
%autosetup -n nabu-firmware-%{version}

%install
mkdir -p %{buildroot}%{_firmwaredir}/qcom/sm8150/xiaomi/nabu \
         %{buildroot}%{_firmwaredir}/cirrus \
         %{buildroot}%{_firmwaredir}/novatek \
         %{buildroot}%{_firmwaredir}/qca \
         %{buildroot}%{_firmwaredir}/ath10k/WCN3990/hw1.0
cp -a a630_sqe.fw a640_gmu.bin %{buildroot}%{_firmwaredir}/qcom
cp -a a640_zap.mbn adsp.mbn cdsp.mbn modem* venus.mbn wlanmdsp.mbn slpi_nb.mbn \
    %{buildroot}%{_firmwaredir}/qcom/sm8150/xiaomi/nabu
cp -a hexagonfs %{buildroot}%{_firmwaredir}/qcom/sm8150/xiaomi/nabu/
cp -a crbtfw32.tlv crnv32.bin %{buildroot}%{_firmwaredir}/qca/
cp -a board-2.bin firmware-5.bin \
    %{buildroot}%{_firmwaredir}/ath10k/WCN3990/hw1.0/
# The generic WMFW contains the CS35L41 protection program.  Each physical
# amplifier has a separate coefficient file selected by its BR/TR/BL/TL
# component prefix in the kernel machine description.
cp -a cs35l41* [BT][LR]-cs35l41* %{buildroot}%{_firmwaredir}/cirrus/
cp -a novatek_nt36523_fw.bin %{buildroot}%{_firmwaredir}/novatek
find %{buildroot}%{_firmwaredir} -type f -exec chmod 0644 {} \;

%check
for required in \
    qcom/sm8150/xiaomi/nabu/slpi_nb.mbn \
    qcom/sm8150/xiaomi/nabu/hexagonfs/sensors/sns_reg.conf \
    qcom/sm8150/xiaomi/nabu/hexagonfs/sensors/registry/lsm6dso_0_platform \
    qca/crbtfw32.tlv qca/crnv32.bin \
    ath10k/WCN3990/hw1.0/board-2.bin \
    ath10k/WCN3990/hw1.0/firmware-5.bin \
    cirrus/cs35l41-dsp1-spk-prot.wmfw \
    cirrus/BR-cs35l41-dsp1-spk-prot.bin \
    cirrus/TR-cs35l41-dsp1-spk-prot.bin \
    cirrus/BL-cs35l41-dsp1-spk-prot.bin \
    cirrus/TL-cs35l41-dsp1-spk-prot.bin; do
    test -s "%{buildroot}%{_firmwaredir}/$required"
done

%files
%{_firmwaredir}/qcom
%{_firmwaredir}/cirrus
%{_firmwaredir}/novatek
%{_firmwaredir}/qca/crbtfw32.tlv
%{_firmwaredir}/qca/crnv32.bin
%{_firmwaredir}/ath10k/WCN3990/hw1.0/board-2.bin
%{_firmwaredir}/ath10k/WCN3990/hw1.0/firmware-5.bin

%changelog
* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 1-3.nabu1
- Install all four prefix-specific CS35L41 speaker-protection coefficient files.
- Reject firmware builds that omit any Nabu amplifier tuning payload.

* Tue Sep 01 2026 mcc45tr <mcc45tr@gmail.com> - 1-2.nabu1
- Package SLPI, Hexagon sensor registry, Bluetooth patch and Nabu Wi-Fi firmware.
- Keep Android calibration read-only; the runtime package copies it to tmpfs.

* Thu Aug 27 2026 SENEMOS COPR <noreply@localhost> - 1-1
- Publish the existing Nabu firmware payload required by nabu-system-integration.
