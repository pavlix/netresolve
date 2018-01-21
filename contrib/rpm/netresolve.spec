Name: netresolve
Version: 0.0.1
Release: 0
Summary: Generic name resolution library
Group: System Environment/Libraries
License: BSD
URL: https://sourceware.org/%{name}/
Source: %{name}-%{version}.tar.xz
BuildRequires: ldns-devel
BuildRequires: pkgconfig(libcares)
BuildRequires: unbound-devel
BuildRequires: pkgconfig(avahi-client)
BuildRequires: pkgconfig(libasyncns)
# live builds
BuildRequires: autoconf automake libtool
# tests
BuildRequires: valgrind
# subpackages installed by netresolve virtual package
Requires: %{name}-core%{?_isa} = %{version}-%{release}
Requires: %{name}-tools%{?_isa} = %{version}-%{release}
Requires: %{name}-compat%{?_isa} = %{version}-%{release}
Requires: %{name}-backends-compat%{?_isa} = %{version}-%{release}
Requires: %{name}-backends-aresdns0%{?_isa} = %{version}-%{release}
Requires: %{name}-backends-avahi%{?_isa} = %{version}-%{release}
%description
Netresolve is a package for non-blocking network name resolution via backends
intended as a replacement for name service switch based name resolution in
glibc as well as a testbed for future glibc improvements.

%package core
Summary: Core netresolve libraries
Group: Development/Libraries
%description core
This package provides core netresolve library with basic name resolution
capabilities for tools and application.

%package compat
Summary: Compatibility netresolve libraries and tools
Group: Development/Libraries
Requires: %{name}-core%{?_isa} = %{version}-%{release}
%description compat
This package provides libraries and tools for using netresolve from applications
built against other name resolution libraries.

%package tools
Summary: Command line tools based on core netresolve libraries
Group: Development/Libraries
Requires: %{name}-core%{?_isa} = %{version}-%{release}
%description tools
This package provides tools that provide netresolve capabilities using the
command line.

%package backends-compat
Summary: Backends for netresolve using existing tools
Requires: %{name}-core%{?_isa} = %{version}-%{release}
Group: Development/Libraries
%description backends-compat
This package provides backends for querying libc, glibc nsswitch backends,
asyncns and other existing name resolution libraries.

%package backends-aresdns0
Summary: DNS backend for netresolve based on aresdns
Group: Development/Libraries
Requires: %{name}-core%{?_isa} = %{version}-%{release}
%description backends-aresdns0
This package provides DNS capabilities including learning DNSSEC validity
from the AD flag for netresolve using c-ares.

%package backends-avahi0
Summary: Multicast DNS backend for netresolve based on libavahi
Group: Development/Libraries
Requires: %{name}-core%{?_isa} = %{version}-%{release}
%description backends-avahi0
This package provides Multicast DNS capabilities using Avahi daemon and
libraries.

%package backends-ubdns0
Summary: DNS backend for netresolve based on libunbound
Group: Development/Libraries
Requires: %{name}-core%{?_isa} = %{version}-%{release}
%description backends-ubdns0
This package provides DNS capabilities including DNSSEC validation to
netresolve using libunbound.

%package devel
Summary: Development files for netresolve
Group: Development/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
%description devel
This package contains header files and libraries needed to compile
applications or shared objects that use netresolve.

%prep
%setup -q
NOCONFIGURE=yes ./autogen.sh

# disable some tests for now
sed -i \
    -e '/999999x/d' \
    -e '/x-x-x-x-x-x-x-x-x/d' \
    tests/test-netresolve.sh

%build
%configure \
    --disable-silent-rules
make %{?_smp_mflags}

%install
%make_install
find %{buildroot} -name '*.la' -delete

#%check
# Checks don't seem to work in different environments
#export NETRESOLVE_TEST_COMMAND="libtool execute valgrind --leak-check=full --error-exitcode=1 ./netresolve"
#make check || { cat ./test-suite.log; false; }

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%post core -p /sbin/ldconfig
%postun core -p /sbin/ldconfig

%post compat -p /sbin/ldconfig
%postun compat -p /sbin/ldconfig

%post backends-compat -p /sbin/ldconfig
%postun backends-compat -p /sbin/ldconfig

%post backends-aresdns0 -p /sbin/ldconfig
%postun backends-aresdns0 -p /sbin/ldconfig

%post backends-ubdns0 -p /sbin/ldconfig
%postun backends-ubdns0 -p /sbin/ldconfig

%post backends-avahi0 -p /sbin/ldconfig
%postun backends-avahi0 -p /sbin/ldconfig

%files core
%license COPYING
%doc README.md
%{_libdir}/libnetresolve-backend-any.so.0
%{_libdir}/libnetresolve-backend-any.so.0.0.0
%{_libdir}/libnetresolve-backend-exec.so.0
%{_libdir}/libnetresolve-backend-exec.so.0.0.0
%{_libdir}/libnetresolve-backend-hostname.so.0
%{_libdir}/libnetresolve-backend-hostname.so.0.0.0
%{_libdir}/libnetresolve-backend-hosts.so.0
%{_libdir}/libnetresolve-backend-hosts.so.0.0.0
%{_libdir}/libnetresolve-backend-loopback.so.0
%{_libdir}/libnetresolve-backend-loopback.so.0.0.0
%{_libdir}/libnetresolve-backend-numerichost.so.0
%{_libdir}/libnetresolve-backend-numerichost.so.0.0.0
%{_libdir}/libnetresolve-backend-unix.so.0
%{_libdir}/libnetresolve-backend-unix.so.0.0.0
%{_libdir}/libnetresolve.so.0
%{_libdir}/libnetresolve.so.0.0.0
%{_libdir}/libnss_netresolve.so.2
%{_libdir}/libnss_netresolve.so.2.0.0

%files tools
%{_bindir}/getaddrinfo
%{_bindir}/gethostbyaddr
%{_bindir}/gethostbyname
%{_bindir}/getnameinfo
%{_bindir}/netresolve
%{_bindir}/res_query

%files compat
%{_bindir}/wrapresolve
%{_libdir}/libnetresolve-asyncns.so.0
%{_libdir}/libnetresolve-asyncns.so.0.0.0
%{_libdir}/libnetresolve-libc.so.0
%{_libdir}/libnetresolve-libc.so.0.0.0

%files backends-compat
%{_libdir}/libnetresolve-backend-asyncns.so.0
%{_libdir}/libnetresolve-backend-asyncns.so.0.0.0
%{_libdir}/libnetresolve-backend-libc.so.0
%{_libdir}/libnetresolve-backend-libc.so.0.0.0
%{_libdir}/libnetresolve-backend-nss.so.0
%{_libdir}/libnetresolve-backend-nss.so.0.0.0


%files backends-aresdns0
%{_libdir}/libnetresolve-backend-aresdns.so.0
%{_libdir}/libnetresolve-backend-aresdns.so.0.0.0

%files backends-avahi0
%{_libdir}/libnetresolve-backend-avahi.so.0
%{_libdir}/libnetresolve-backend-avahi.so.0.0.0

%files backends-ubdns0
%{_libdir}/libnetresolve-backend-ubdns.so.0
%{_libdir}/libnetresolve-backend-ubdns.so.0.0.0

%files devel
%{_includedir}/netresolve-epoll.h
%{_includedir}/netresolve-epoll.h
%{_includedir}/netresolve-event.h
%{_includedir}/netresolve-event.h
%{_includedir}/netresolve-glib.h
%{_includedir}/netresolve-glib.h
%{_includedir}/netresolve-nonblock.h
%{_includedir}/netresolve-nonblock.h
%{_includedir}/netresolve-select.h
%{_includedir}/netresolve-select.h
%{_includedir}/netresolve.h
%{_libdir}/libnetresolve-asyncns.so
%{_libdir}/libnetresolve-backend-any.so
%{_libdir}/libnetresolve-backend-aresdns.so
%{_libdir}/libnetresolve-backend-asyncns.so
%{_libdir}/libnetresolve-backend-avahi.so
%{_libdir}/libnetresolve-backend-avahi.so
%{_libdir}/libnetresolve-backend-exec.so
%{_libdir}/libnetresolve-backend-hostname.so
%{_libdir}/libnetresolve-backend-hosts.so
%{_libdir}/libnetresolve-backend-libc.so
%{_libdir}/libnetresolve-backend-libc.so
%{_libdir}/libnetresolve-backend-loopback.so
%{_libdir}/libnetresolve-backend-nss.so
%{_libdir}/libnetresolve-backend-numerichost.so
%{_libdir}/libnetresolve-backend-ubdns.so
%{_libdir}/libnetresolve-backend-unix.so
%{_libdir}/libnetresolve-libc.so
%{_libdir}/libnetresolve.so
%{_libdir}/libnss_netresolve.so
