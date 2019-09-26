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
%autosetup -v -N -S git_am
%autopatch -v -M 0
%autopatch -v -m 1 -M 1
%autopatch -v -m 2 -M 2

%build
%make_build CFLAGS="$RPM_OPT_FLAGS"

%install
%make_install

%files
%doc	FAQ
/usr/local/bin/hello
