Name: hello
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Simple rpm demonstration.

%sourcelist
hello-1.0.tar.gz

%patchlist
hello-1.0-modernize.patch
hello-1.0-install.patch

%description
Simple rpm demonstration.

%prep
%autosetup

%build
%make_build CFLAGS="$RPM_OPT_FLAGS"

%install
%make_install

%files
%doc	FAQ
/usr/local/bin/hello

