Name: hello
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Simple rpm demonstration.

%sourcelist
hello-1.0.tar.gz

%patchlist -f patchlist

%description
Simple rpm demonstration.

%prep
%setup -q
%patch0 -p1 -b .modernize
%patch1 -p1 -b .install

%build
%make_build CFLAGS="$RPM_OPT_FLAGS"

%install
%make_install

%files
%doc	FAQ
/usr/local/bin/hello

