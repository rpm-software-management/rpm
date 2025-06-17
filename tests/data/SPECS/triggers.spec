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

%triggerprein -e -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERPREIN $*
%%{?failtriggerprein:exit 1}

%triggerin -e -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERIN $*
%%{?failtriggerin:exit 1}

%triggerun -e -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERUN $*
%%{?failtriggerun:exit 1}

%triggerpostun -e -- %{trigpkg}
echo %{name}-%{version}-%{release} TRIGGERPOSTUN $*
%%{?failtriggerpostun:exit 1}

