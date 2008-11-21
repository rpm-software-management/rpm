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
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_datadir}
echo "%{filedata}" > $RPM_BUILD_ROOT/%{_datadir}/my.version

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_datadir}/my.version
