Name:           ifmultiline
Version:         1.0
Release:        1
Group:          Testing
License:        GPL
BuildArch:      noarch
Summary: Test multiline if conditions

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/a
echo "x" > $RPM_BUILD_ROOT/a/empty\
Caps1

%if 1 && ( 9|| \
1)

%endif

%files -n %{name}
/a/emptyCaps1


%changelog