# avoid depending on rpm configuration
%define _datadir /usr/share

Name:		conflict%{pkg}
Version:	1.0
Release:	1
Summary:	Testing file conflict behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
mkdir -p $RPM_BUILD_ROOT/%{_datadir}
echo "%{filedata}" > $RPM_BUILD_ROOT/%{_datadir}/my.version

%files
%defattr(-,root,root,-)
%{?fileattr} %{_datadir}/my.version
