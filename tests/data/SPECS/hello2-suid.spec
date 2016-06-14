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
Excludearch: lsi
Excludeos: cpm
Provides: hi
Conflicts: goodbye
Obsoletes: howdy
Prefix: /usr

%description
Simple rpm demonstration.

%prep
%setup -q -n hello-1.0
%patch0 -p1 -b .modernize

%build
make CFLAGS="-g -O1"
mv hello hello2
make CFLAGS="-g -O2 -D_FORTIFY_SOURCE=2"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
# Note explicit install hello as suid
install -m 4755 hello $RPM_BUILD_ROOT/usr/local/bin
install -m 755 hello2 $RPM_BUILD_ROOT/usr/local/bin

%clean
rm -rf $RPM_BUILD_ROOT

%pre

%post

%preun

%postun

%files
# Note we don't set any attrs. We expect the suid flag to have been picked up.
/usr/local/bin/hello
/usr/local/bin/hello2
%defattr(-,root,root)
%doc	FAQ
#%readme README
#%license COPYING

%changelog
* Tue Jun 14 2016 Mark Wielaard <mjw@redhat.com>
- Make hello (implicit) suid

* Wed May 18 2016 Mark Wielaard <mjw@redhat.com>
- Add hello2 for dwz testing support.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
