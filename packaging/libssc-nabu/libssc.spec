%global debug_package %{nil}
%global upstream_name libssc

Name:           libssc-nabu
Version:        0.4.4
Release:        1.nabu1.test%{?dist}
Summary:        Qualcomm Sensor Core client library for Nabu sensor services

License:        GPL-3.0-or-later
URL:            https://codeberg.org/DylanVanAssche/libssc
Source0:        %{url}/archive/v%{version}.tar.gz#/%{upstream_name}-%{version}.tar.gz
Patch0:         0001-libssc-avoid-use-after-free-in-sensor-error-logging.patch

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

%changelog
* Mon Aug 31 2026 mcc45tr <mcc45tr@gmail.com> - 0.4.4-1.nabu1.test
- Rebase on upstream 0.4.4 and its corrected Python installed-test layout.
- Retain the Nabu GError ownership fix used during SLPI failure recovery.
- Preserve the libssc and python3-ssc compatibility provides.
