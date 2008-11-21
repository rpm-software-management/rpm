Name:		versiontest
Version:	%{ver}
Release:	1
Summary:	Testing version behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
