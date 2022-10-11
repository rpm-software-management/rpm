Name:		versiontest
Version:	%{ver}
Release:	1
%if %{defined:epoch}
Epoch:		%{epoch}
%endif
Summary:	Testing version behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%files
%defattr(-,root,root)
