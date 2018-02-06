Summary: hello -- hello, world rpm
Name: hello
Version: 1.0
Release: 2
Group: Utilities
License: GPL
Distribution: RPM test suite.
Vendor: Red Hat Software
Packager: Red Hat Software <bugs@redhat.com>
URL: http://www.redhat.com
Source0: hello-1.0.tar.gz
Patch0: hello-1.0-modernize.patch
Prefix: /usr

%description
Simple rpm demonstration.

%prep
%setup -q
%patch0 -p1 -b .modernize

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
* Wed Jun  8 2016 Mark Wielaard <mjw@redhat.com>
- Update release for unique build-id generation tests.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
