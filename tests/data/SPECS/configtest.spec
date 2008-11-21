Name:		configtest
Version:	%{ver}
Release:	1
Summary:	Testing config file behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}
echo "%{filedata}" > $RPM_BUILD_ROOT/%{_sysconfdir}/my.conf

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config %{_sysconfdir}/my.conf
