Name:           bcondtest
Version:        1.0
Release:        1
Group:          Testing
License:        CC0
BuildArch:      noarch
Summary:        Test package for the bcond macro

%bcond normally_on 1
%bcond normally_off 0
%bcond both_features %[%{with normally_on} && %{with normally_off}]

%if %{with normally_on}
Provides:       has_bcond(normally_on)
%endif
%if %{with normally_off}
Provides:       has_bcond(normally_off)
%endif
%if %{with both_features}
Provides:       has_bcond(both_features)
%endif

%description
%{summary}

%install
mkdir -p %{buildroot}/opt
touch %{buildroot}/opt/file

%files
/opt/file

%changelog
