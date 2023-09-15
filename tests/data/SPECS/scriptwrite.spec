Name:           scriptwrite
Version:        1.0
Release:        1
Summary:        Testing script running environment
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%files

%pre
echo "%{name}-%{version} pre" > /tmp/%{name}.log

%post
echo "%{name}-%{version} post" >> /tmp/%{name}.log
