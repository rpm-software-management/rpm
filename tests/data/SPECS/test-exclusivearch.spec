Summary:	test
Name:		test
Version:	1
Release:	1
License:	GPL
Group:		Applications/System
%ifarch x86_64
ExclusiveArch:	aarch64
%else
ExclusiveArch:	x86_64
%endif


%description
test

%prep
echo prep

%build
echo build

%install
echo install
