Name: hlbreak
Version: %{ver}
Release: 0
License: GPL
Summary: Testing changing hardlink behavior
BuildArch: noarch

%description
%{summary}

%install
mkdir -p %{buildroot}/opt
echo "content" > %{buildroot}/opt/file2
%if %{ver} == 1
ln %{buildroot}/opt/file2 %{buildroot}/opt/file1
%endif

%files
%if %{ver} == 1
/opt/file1
%endif
/opt/file2
