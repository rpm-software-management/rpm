Summary: hello2 -- double hello, world rpm
Name: hello2
Version: 1.0
Release: 1
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
%setup -q -n hello-1.0
%patch0 -p1 -b .modernize

%build
make CFLAGS="-g -O1"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
make DESTDIR=$RPM_BUILD_ROOT install
ln $RPM_BUILD_ROOT/usr/local/bin/hello $RPM_BUILD_ROOT/usr/local/bin/hello2

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
%attr(0751,root,root)	/usr/local/bin/hello2

%changelog
* Mon Jun  6 2016 Mark Wielaard <mjw@redhat.com>
- Hard link hello to hello2 for duplicate build-id testing.

* Wed May 18 2016 Mark Wielaard <mjw@redhat.com>
- Add hello2 for dwz testing support.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
