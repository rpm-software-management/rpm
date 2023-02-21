Name: hello
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Simple rpm demonstration.

Source0: hello-1.0.tar.gz
Patch0: hello-1.0-install.patch
Patch1: hello-1.0-modernize.patch

%description
Simple rpm demonstration.

%prep
%setup -q
%patch0 -p1 -b .install
%patch1 -p1 -b .modernize

%changelog
