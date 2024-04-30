Name: templ
Version: 1
Release: 1
License: Public domain
Summary: Spec template tests
BuildArch: noarch

%description
%{summary}

%build
echo BUILD ${RPM_ARCH} %{_arch} %{_target_cpu} >> /tmp/build.out
