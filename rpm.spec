Name:           bolo-collectors
Version:        0.4.7
Release:        1%{?dist}
Summary:        Monitoring System Collectors

Group:          Applications/System
License:        GPLv3+
URL:            https://github.com/bolo/bolo-collectors
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  gcc
BuildRequires:  libctap-devel
BuildRequires:  pcre-devel
BuildRequires:  zeromq-devel
BuildRequires:  libvigor-devel
BuildRequires:  libcurl-devel
BuildRequires:  mysql-devel

%description
bolo is a lightweight and scalable monitoring system that can
track samples, counters, states and configuration data.

This package provides collectors for system metrics.

%prep
%setup -q


%build
%configure --with-all-collectors --prefix=/usr PERLDIR=/usr/share/perl5
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_bindir}/rrdq
%{_libdir}/bolo/collectors/cogd
%{_libdir}/bolo/collectors/files
%{_libdir}/bolo/collectors/hostinfo
%{_libdir}/bolo/collectors/httpd
%{_libdir}/bolo/collectors/linux
%{_libdir}/bolo/collectors/mysql
%{_libdir}/bolo/collectors/nagwrap
%{_libdir}/bolo/collectors/netstat
%{_libdir}/bolo/collectors/process
%{_libdir}/bolo/collectors/snmp_cisco
%{_libdir}/bolo/collectors/snmp_cisco_detect
%{_libdir}/bolo/collectors/snmp_cisco_sys
%{_libdir}/bolo/collectors/snmp_cisco_ifaces
%{_libdir}/bolo/collectors/snmp_ifaces
%{_libdir}/bolo/collectors/snmp_system
%{_libdir}/bolo/collectors/tcp

%changelog
* Wed Jul 29 2015 James Hunt <james@niftylogic.com> 0.4.7-1
- New release

* Thu Jul 23 2015 James Hunt <james@niftylogic.com> 0.4.6-1
- New release

* Wed Jul 22 2015 James Hunt <james@niftylogic.com> 0.4.5-1
- New release

* Tue Jul 21 2015 James Hunt <james@niftylogic.com> 0.4.4-1
- New release

* Tue Jul 21 2015 James Hunt <james@niftylogic.com> 0.4.3-1
- New release

* Mon Jul 20 2015 James Hunt <james@niftylogic.com> 0.4.1-1
- New release

* Mon Jul 20 2015 James Hunt <james@niftylogic.com> 0.4.0-1
- New release

* Wed Jul 15 2015 James Hunt <james@niftylogic.com> 0.3.0-1
- New release

* Fri May 22 2015 James Hunt <james@niftylogic.com> 0.1.0-1
- Initial RPM package
