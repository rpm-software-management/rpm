Name: hello
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Simple rpm demonstration.

%sourcelist
hello-1.0.tar.gz
buildrequires-1.0.tar.gz

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

%{echo:RESULTS patches:%{patches}}
%{echo:RESULTS patches -M 0:%{patches -M 0}}
%{echo:RESULTS patches -m 1 -M 1:%{patches -m 1 -M 1}}
%{echo:RESULTS patches -m 2:%{patches -m2}}
%{echo:RESULTS sources:%{sources}}
%{echo:RESULTS sources -M 0:%{sources -M 0}}
%{echo:RESULTS sources -m 1 -M 1:%{sources -m 1 -M 1}}
%{echo:RESULTS sources -m 2:%{sources -m2}}
