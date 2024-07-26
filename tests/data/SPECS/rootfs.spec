Name:           rootfs
Version:        1.0
Release:        1
Summary:        Package owning top-level root directory
License:        GPL
BuildArch:      noarch
Prefix:         /

%description
%{summary}.

%install
mkdir -p $RPM_BUILD_ROOT/foo
touch $RPM_BUILD_ROOT/foo/bar

%files
/
