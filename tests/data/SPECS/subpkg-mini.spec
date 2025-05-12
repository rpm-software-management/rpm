%bcond_with pkgfmt

%if %{with pkgfmt}
%define _rpmformat %{_myrpmformat}
%endif

Name:           subpkg-mini
Version:        1.0
Release:        1
Summary:        Test
License:        Public Domain

%description
%{summary}.

%package test2
Summary: Test2.
%description test2

%package test3
Summary: Test3.
%description test3

%files

%files test2

%files test3
