%global debug_package %{nil}
%global upstream_name hexagonrpc
%global hotdog_commit 8ab7a510eec3614fb3021bf2595c0f9e475009bb

Name:           hexagonrpc-nabu
Version:        0.5.0
Release:        1.nabu1.test%{?dist}
Summary:        Qualcomm FastRPC userspace bridge for Xiaomi Pad 5
License:        GPL-3.0-or-later
URL:            https://github.com/linux-msm/hexagonrpc
Source0:        https://github.com/linux-msm/%{upstream_name}/archive/refs/tags/v%{version}.tar.gz#/%{upstream_name}-%{version}.tar.gz
Source1:        hexagonrpcd-adsp-rootpd.service
Source2:        hexagonrpcd-adsp-sensorspd.service
Source3:        hexagonrpcd-sdsp.service
Source4:        sysusers.conf
Source5:        10-fastrpc.rules

# Adapted directly from the OnePlus 7 Pro hotdog bring-up at the pinned commit.
# Keep local, checksummed copies so an upstream update cannot silently replace
# the Nabu merge layer.
Patch0:         0001-serve-writable-files.patch
Patch1:         0002-implement-fremove.patch
Patch2:         0003-raise-listener-input-limit.patch
Patch3:         0004-support-extended-frename.patch

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  systemd-rpm-macros
Requires(post): systemd
Provides:       hexagonrpc = %{version}-%{release}
Obsoletes:      hexagonrpc < %{version}-%{release}

%{?sysusers_requires_compat}

%description
Userspace FastRPC bridge and system services for Qualcomm remote processors on
Xiaomi Pad 5 (nabu). The Nabu build adds the writable file operations and
larger listener input buffer required by the DSP-hosted sensor registry. The
daemon remains unprivileged and its SDSP service is restricted to the Nabu
hexagonfs root.

%package -n hexagonrpc-nabu-devel
Summary:        Libraries and headers for Nabu HexagonRPC development
Requires:       %{name} = %{version}-%{release}
Provides:       hexagonrpc-devel = %{version}-%{release}
Obsoletes:      hexagonrpc-devel < %{version}-%{release}

%description -n hexagonrpc-nabu-devel
Headers and development files for software integrating with the Nabu FastRPC
userspace bridge.

%prep
%autosetup -n %{upstream_name}-%{version} -p1

%build
%meson
%meson_build

%install
%meson_install

# Since 0.5.0 upstream also installs generic units below %%{_libdir}, discard
# those copies and install the Nabu-specific units in Fedora's unit directory.
rm -rf %{buildroot}%{_libdir}/systemd

install -d %{buildroot}%{_includedir}
cp -a include/libhexagonrpc %{buildroot}%{_includedir}/

install -Dm0644 %{SOURCE1} %{buildroot}%{_unitdir}/hexagonrpcd-adsp-rootpd.service
install -Dm0644 %{SOURCE2} %{buildroot}%{_unitdir}/hexagonrpcd-adsp-sensorspd.service
install -Dm0644 %{SOURCE3} %{buildroot}%{_unitdir}/hexagonrpcd-sdsp.service
install -Dm0644 %{SOURCE4} %{buildroot}%{_sysusersdir}/fastrpc.conf
install -Dm0644 %{SOURCE5} %{buildroot}%{_udevrulesdir}/10-fastrpc.rules

%check
%meson_test

%pre
%sysusers_create_compat %{SOURCE4}

%post
%systemd_post hexagonrpcd-adsp-rootpd.service hexagonrpcd-adsp-sensorspd.service hexagonrpcd-sdsp.service

%preun
%systemd_preun hexagonrpcd-adsp-rootpd.service hexagonrpcd-adsp-sensorspd.service hexagonrpcd-sdsp.service

%postun
%systemd_postun_with_restart hexagonrpcd-adsp-rootpd.service hexagonrpcd-adsp-sensorspd.service hexagonrpcd-sdsp.service

%files
%doc README.md
%license COPYING
%{_unitdir}/*.service
%{_bindir}/hexagonrpcd
%{_libexecdir}/hexagonrpc
%{_libdir}/libhexagonrpc.so.*
%{_sysusersdir}/fastrpc.conf
%{_udevrulesdir}/10-fastrpc.rules
%{_mandir}/man1/hexagonrpcd.1*

%files -n hexagonrpc-nabu-devel
%{_includedir}/libhexagonrpc
%{_libdir}/libhexagonrpc.so

%changelog
* Mon Aug 31 2026 mcc45tr <mcc45tr@gmail.com> - 0.5.0-1.nabu1.test
- Rebase on upstream 0.5.0 while retaining all four Nabu FastRPC merge patches.
- Keep the Nabu service topology, unprivileged account and SDSP registry root.
- Remove duplicate upstream units installed below libdir.

* Tue Aug 25 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.0-103.nabu3.test
- Add DSP-served file write, remove and extended rename operations from the
  pinned Hotdog bring-up patch set.
- Increase the listener input buffer from 256 bytes to 64 KiB and run the
  upstream Meson tests during the RPM build.
- Preserve the unprivileged fastrpc service and Nabu-specific SDSP root.
