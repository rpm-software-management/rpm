Name:           triggers
Version:        1.0
Release:        %{rel}
Summary:        Testing trigger behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%files
%defattr(-,root,root,-)

%triggerprein -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERPREIN $*

%triggerin -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERIN $*

%triggerun -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERUN $*

%triggerpostun -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERPOSTUN $*

