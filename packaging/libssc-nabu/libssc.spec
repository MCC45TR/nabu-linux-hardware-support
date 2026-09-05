%global debug_package %{nil}
%global upstream_name libssc

Name:           libssc-nabu
Version:        0.4.4
Release:        13.nabu12.test%{?dist}
Summary:        Qualcomm Sensor Core client library for Nabu sensor services

License:        GPL-3.0-or-later
URL:            https://codeberg.org/DylanVanAssche/libssc
Source0:        %{url}/archive/v%{version}.tar.gz#/%{upstream_name}-%{version}.tar.gz
Patch0:         0001-libssc-avoid-use-after-free-in-sensor-error-logging.patch
Patch1:         0002-libssc-accept-fractional-and-integer-mount-matrices.patch
Patch2:         0003-ssccli-probe-arbitrary-data-types.patch
Patch3:         0004-libssc-treat-zero-placement-as-unspecified.patch
Patch4:         0005-libssc-add-TCS3701-CCT-sensor-support.patch
Patch5:         0006-libssc-use-standard-measurement-id-for-CCT.patch
Patch6:         0007-libssc-decode-packed-CCT-standard-event.patch
Patch7:         0008-libssc-address-named-light-sensor-data-types.patch
Patch8:         0009-ssccli-expose-LSM6DSO-temperature-stream.patch
Patch9:         0010-ssccli-expose-LSM6DSO-motion-detect-events.patch

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  python3-devel
BuildRequires:  python3-gobject-base
BuildRequires:  python3-protobuf
BuildRequires:  /usr/bin/protoc
BuildRequires:  /usr/bin/protoc-c
BuildRequires:  systemd
BuildRequires:  pkgconfig(libprotobuf-c)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gudev-1.0)
BuildRequires:  pkgconfig(qmi-glib)
BuildRequires:  pkgconfig(qrtr)
BuildRequires:  pkgconfig(udev)
Provides:       libssc = %{version}-%{release}
Obsoletes:      libssc < %{version}-%{release}

%description
Client library for sensors managed by Qualcomm Sensor Core on the Xiaomi Pad 5
(nabu). The Nabu merge layer keeps iio-sensor-proxy alive when SLPI disappears
by completing asynchronous failures without dereferencing an owned GError.

%package -n libssc-nabu-devel
Summary:        Development headers for libssc
Requires:       %{name}%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}
Provides:       libssc-devel = %{version}-%{release}
Obsoletes:      libssc-devel < %{version}-%{release}

%description -n libssc-nabu-devel
Development headers and pkg-config metadata for applications using the Nabu
Qualcomm Sensor Core client library.

%package -n python3-ssc-nabu
Summary:        Python bindings and mock server for libssc
Requires:       %{name}%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires:       python3-gobject-base
Requires:       python3-protobuf
Provides:       python3-ssc = %{version}-%{release}
Obsoletes:      python3-ssc < %{version}-%{release}

%description -n python3-ssc-nabu
Python 3 bindings and the installed-test mock server for the Nabu Qualcomm
Sensor Core client library.

%prep
%autosetup -p1 -n %{upstream_name}

%build
%meson
%meson_build

%install
%meson_install

%check
%meson_test

# Keep the hardware-discovery interface and the already-supported gyroscope
# visible as release contracts. The generic probe discovers only; it never
# enables an unknown stream with a guessed decoder.
grep -F 'probe-data-type' src/libssc-cli.c
grep -F "'gyroscope'" src/libssc-cli.c
grep -F 'Z=%f rad/s' src/libssc-cli.c
grep -F 'SSC placement matrix is unspecified (all zero), using identity matrix' src/libssc-sensor.c
grep -F 'SSC placement matrix is incomplete, falling back to identity matrix' src/libssc-sensor.c
! grep -Fq 'Mount matrix provided by firmware is incomplete or all 0' src/libssc-sensor.c
grep -F 'msg_id != SSC_MSG_REPORT_MEASUREMENT' src/libssc-sensor-cct.c
! grep -Fq 'SSC_MSG_REPORT_MEASUREMENT_CCT' src/libssc-common-private.h
grep -F 'SSC_SENSOR_DATA_TYPE, "cct_front"' src/libssc-sensor-cct.c
grep -F 'repeated float data = 1 [packed = true];' data/ssc-sensor-cct.proto
grep -F 'ctx->cct = msg->data[0];' src/libssc-sensor-cct.c
grep -F 'ssc_sensor_light_new_for_data_type_sync' src/libssc-sensor-light.c
grep -F '"ambient_light_back"' src/libssc-cli.c
grep -F '"sensor_temperature"' src/libssc-cli.c
grep -F 'LSM6DSO temperature measurement:' src/libssc-cli.c
grep -F '"motion_detect"' src/libssc-cli.c
grep -F 'LSM6DSO motion-detect event:' src/libssc-cli.c

%files
%license LICENSE
%{_bindir}/ssccli
%{_libdir}/%{upstream_name}.so.2

%files -n libssc-nabu-devel
%{_includedir}/%{upstream_name}
%{_libdir}/%{upstream_name}.so
%{_libdir}/pkgconfig/%{upstream_name}.pc

%files -n python3-ssc-nabu
%{python3_sitelib}/ssc_server/
%dir %{_libexecdir}/installed-tests
%dir %{_libexecdir}/installed-tests/%{upstream_name}
%{_libexecdir}/installed-tests/%{upstream_name}/ssc-server

%posttrans
if [ -x /usr/bin/systemctl ]; then
    /usr/bin/systemctl try-restart nabu-cct-iio-bridge.service iio-sensor-proxy.service >/dev/null 2>&1 || :
fi

%changelog
* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-13.nabu12.test
- Expose the firmware-provided LSM6DSO motion-detect on-change stream without
  inventing a polling rate, threshold or device-local policy.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-12.nabu11.test
- Expose the LSM6DSO internal temperature stream under its actual Qualcomm SSC
  data type and print typed Celsius samples through ssccli.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-11.nabu10.test
- Export the named light constructors through libssc's version script so
  consumers and the diagnostic CLI can link on all supported architectures.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-10.nabu9.test
- Allow clients to address a named SSC ambient-light stream while preserving
  the existing default API.
- Add an ssccli light-back diagnostic for Nabu's rear BU27030 sensor.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-9.nabu8.test
- Decode the packed three-float sns_std_sensor_event emitted by cct_front.
- Publish its measured Kelvin value while preserving chromaticity coordinates.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-8.nabu7.test
- Restart Nabu sensor consumers after the corrected CCT decoder is installed.

* Sat Sep 05 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-7.nabu6.test
- Decode TCS3701 cct_front reports carried by Qualcomm SSC's standard
  measurement event ID instead of waiting for a non-existent CCT-specific ID.

* Thu Sep 03 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-6.nabu5.test
- Add a typed cct_front client for the TCS3701 sns_cct protocol.
- Expose Kelvin, calibrated lux, raw RGB/C/W channels, chromaticity, IR ratio,
  gain and integration time without mis-decoding the ambient-light stream.

* Thu Sep 03 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-5.nabu4.test
- Correct the zero-placement patch hunk length so RPM can apply it strictly.

* Thu Sep 03 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-4.nabu3.test
- Treat Nabu's all-zero Qualcomm placement array as an unspecified placement.
- Keep identity semantics without emitting a false firmware warning at startup.
- Retain warnings for genuinely incomplete SSC placement arrays.

* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-3.nabu2.test
- Add safe discovery-only probing for arbitrary SSC data types.
- Advertise the existing gyroscope CLI support and report angular velocity in rad/s.

* Wed Sep 02 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-2.nabu1.test
- Preserve valid fractional SSC mount matrices instead of truncating them.
- Accept both floating-point and integer firmware matrix coefficients.
- Fall back to identity only for incomplete or genuinely all-zero matrices.

* Mon Aug 31 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-1.nabu1.test
- Rebase on upstream 0.4.4 and its corrected Python installed-test layout.
- Retain the Nabu GError ownership fix used during SLPI failure recovery.
- Preserve the libssc and python3-ssc compatibility provides.
