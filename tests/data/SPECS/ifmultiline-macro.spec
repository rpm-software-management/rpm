Name:           ifmultiline-macro
Version:        1.0
Release:        1
Group:          Testing
License:        GPL
BuildArch:      noarch
Summary:        Test multiline if conditions

%description
%{summary}

%if 0

%define test() \
%if 1 \
%{echo:hello} \
%endif \
%nil

%endif

%changelog
