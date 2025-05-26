%bcond_with genbr

Name: minibr
Version: 1
Release: 1
License: k
Summary: Minimal spec

%if %{with genbr}
%generate_buildrequires
echo rpm-build
%endif

%description

%files
