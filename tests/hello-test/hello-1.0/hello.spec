Summary: hello -- hello, world rpm
Name: hello
Version: 1.0
Release: 1
Group: Utilities
License: GPL
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

%files
%defattr(-,root,root)
/usr/local/bin/hello
