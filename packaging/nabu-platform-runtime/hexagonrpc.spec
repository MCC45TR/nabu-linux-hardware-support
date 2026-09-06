%global debug_package %{nil}
%global upstream_name hexagonrpc
%global hotdog_commit 8ab7a510eec3614fb3021bf2595c0f9e475009bb
%global iris_commit 9be612d394a4622f9393ab9645698fe40f818c02

Name:           hexagonrpc-nabu
Version:        0.5.0
Release:        4.nabu1.test%{?dist}
Summary:        Qualcomm FastRPC userspace bridge for Xiaomi Pad 5
License:        GPL-3.0-or-later
URL:            https://github.com/linux-msm/hexagonrpc
Source0:        https://github.com/linux-msm/%{upstream_name}/archive/refs/tags/v%{version}.tar.gz#/%{upstream_name}-%{version}.tar.gz
Source1:        hexagonrpcd-adsp-rootpd.service
Source2:        hexagonrpcd-adsp-sensorspd.service
Source3:        hexagonrpcd-sdsp.service
Source4:        sysusers.conf
Source5:        10-fastrpc.rules
Source6:        iris-vaapi-%{iris_commit}.tar.gz

# Adapted directly from the OnePlus 7 Pro hotdog bring-up at the pinned commit.
# Keep local, checksummed copies so an upstream update cannot silently replace
# the Nabu merge layer.
Patch0:         0001-serve-writable-files.patch
Patch1:         0002-implement-fremove.patch
Patch2:         0003-raise-listener-input-limit.patch
Patch3:         0004-support-extended-frename.patch
Patch4:         0005-keep-listener-alive-on-stale-requests.patch

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  make
BuildRequires:  systemd-rpm-macros
BuildRequires:  pkgconfig(libva)
BuildRequires:  pkgconfig(libva-drm)
BuildRequires:  pkgconfig(vulkan)
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

%package -n iris-vaapi-nabu
Summary:        Experimental VA-API driver for Qualcomm SM8150 Iris1
License:        GPL-2.0-or-later
Requires:       libva
Requires:       systemd-udev

%description -n iris-vaapi-nabu
The CFM880 experimental VA-API userspace driver for the Qualcomm SM8150 Iris1
stateful V4L2 decoder used by Xiaomi Pad 5. It remains a separately named
binary RPM while sharing the consolidated Nabu platform-runtime source build.

%prep
%autosetup -n %{upstream_name}-%{version} -p1
tar -xf %{SOURCE6}

%build
%meson
%meson_build
%make_build -C iris-vaapi-%{iris_commit}

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

make -C iris-vaapi-%{iris_commit} install \
    DESTDIR=%{buildroot} DRIVERDIR=%{_libdir}/dri
ln -s iris_drv_video.so %{buildroot}%{_libdir}/dri/msm_drv_video.so
install -Dm0644 iris-vaapi-%{iris_commit}/tools/99-iris-dmaheap.rules \
    %{buildroot}%{_prefix}/lib/udev/rules.d/99-iris-dmaheap.rules
install -Dm0644 iris-vaapi-%{iris_commit}/tools/99-iris-vaapi.conf \
    %{buildroot}%{_prefix}/lib/modprobe.d/99-iris-vaapi.conf
install -Dm0644 iris-vaapi-%{iris_commit}/tools/90-iris-vaapi-nabu.conf \
    %{buildroot}%{_prefix}/lib/environment.d/90-iris-vaapi-nabu.conf

%check
%meson_test
make -C iris-vaapi-%{iris_commit} check
test -s %{buildroot}%{_libdir}/dri/iris_drv_video.so
test "$(readlink %{buildroot}%{_libdir}/dri/msm_drv_video.so)" = iris_drv_video.so
grep -Fxq 'options qcom_iris allow_fw_boot=1 cached_capture=1' \
    %{buildroot}%{_prefix}/lib/modprobe.d/99-iris-vaapi.conf
grep -Fxq 'LIBVA_DRIVER_NAME=iris' \
    %{buildroot}%{_prefix}/lib/environment.d/90-iris-vaapi-nabu.conf
! grep -E '"/dev/video0"' iris-vaapi-%{iris_commit}/src/decode.c \
    iris-vaapi-%{iris_commit}/src/iris_vaapi.c \
    iris-vaapi-%{iris_commit}/src/v4l2_dec.c

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

%files -n iris-vaapi-nabu
%license iris-vaapi-%{iris_commit}/COPYING
%doc iris-vaapi-%{iris_commit}/README.md iris-vaapi-%{iris_commit}/docs/
%{_libdir}/dri/iris_drv_video.so
%{_libdir}/dri/msm_drv_video.so
%{_prefix}/lib/udev/rules.d/99-iris-dmaheap.rules
%{_prefix}/lib/modprobe.d/99-iris-vaapi.conf
%{_prefix}/lib/environment.d/90-iris-vaapi-nabu.conf

%changelog
* Sun Sep 06 2026 mcc45tr <mcc45tr@gmail.com> - 0.5.0-4.nabu1.test
- Consolidate HexagonRPC and Iris VA-API into one COPR source family while
  preserving the existing runtime and development binary RPM names.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.5.0-3.nabu1.test
- Rebase the resume-safety fix after the existing Nabu extended-listener
  patches and clear consumed buffers between reverse-tunnel transactions.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.5.0-2.nabu1.test
- Keep the SDSP reverse tunnel alive when firmware sends a stale request after
  resume and clear consumed listener buffers before the next transaction.

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
