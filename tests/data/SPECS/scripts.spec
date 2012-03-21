Name:           scripts
Version:        1.0
Release:        %{rel}
Summary:        Testing script behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%files
%defattr(-,root,root,-)

%pretrans
echo %{name}-%{version}-%{release} PRETRANS $*

%pre
echo %{name}-%{version}-%{release} PRE $*

%post
echo %{name}-%{version}-%{release} POST $*

%preun
echo %{name}-%{version}-%{release} PREUN $*

%postun
echo %{name}-%{version}-%{release} POSTUN $*

%posttrans
echo %{name}-%{version}-%{release} POSTTRANS $*
