Summary: hello -- hello, world rpm
Name: hello
Version: 1.0
Release: 1
Group: Utilities
License: GPL
URL: http://www.redhat.com
Source0: hello-1.0.tar.gz
BuildRoot: /var/tmp/hello-root

%description
Simple rpm demonstration.

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%pre

%post

%preun

%postun

%files
%defattr(-,root,root)
%doc	FAQ
#%readme README
#%license COPYING
%attr(0751,root,root)	/usr/local/bin/hello

%changelog
* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
