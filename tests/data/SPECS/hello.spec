# To whom it may concern:
# DO NOT COPY THIS SPEC or its derivates to your testcase. That is,
# unless your test-case actually requires running a compiler and
# inspecting it's output. Everybody else should use a simple noarch
# package which can be built under runroot in the test-suite.

Summary: hello -- hello, world rpm
 Name: hello
Version: 1.0
	Release: 1
 Group: Utilities
License: GPL
SourceLicense: GPL, ASL 1.0
Distribution: RPM test suite.
URL: http://rpm.org
	Source0: hello-1.0.tar.gz
 Patch0: hello-1.0-modernize.patch
Prefix: /usr

%description
Simple rpm demonstration.

%prep
%setup -q
%patch -p1 -b .modernize 0

%build
make

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
%doc	FAQ
#%readme README
#%license COPYING
%attr(0751,root,root)	/usr/local/bin/hello

%changelog
* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
