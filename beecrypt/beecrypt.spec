Summary: The BeeCrypt Cryptography Library
Name: beecrypt
Version: 2.1.0
Release: 1
Copyright: LGPL
Group: Development/Libraries
Source0: http://beecrypt.virtualunlimited.com/download/beecrypt-%{version}.tar.gz
URL: http://beecrypt.virtualunlimited.com/
Buildroot: %{_tmppath}/%{name}-root

%description
The BeeCrypt Cryptography Library.

%package devel
Summary: The BeeCrypt Cryptography Library headers
Group: Development/Libraries
Requires: beecrypt = %{version}

%description devel
The BeeCrypt Cryptography Library headers.

%prep
%setup -q

%build
%configure
make

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR="${RPM_BUILD_ROOT}" install

%clean
rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%{_libdir}/libbeecrypt.so.2.1.0

%files devel
%defattr(-,root,root)
%{_libdir}/libbeecrypt.so.2
%{_libdir}/libbeecrypt.so
%{_libdir}/libbeecrypt.la
%{_includedir}/beecrypt

%changelog
* Tue Sep 18 2001 Jeff Johnson <jbj@redhat.com>
- repackage
