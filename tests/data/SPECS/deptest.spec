Name:		deptest-%{pkg}
Version:	1.0
Release:	1
Summary:	Testing dependency behavior

Group:		Testing
License:	GPL
BuildArch:	noarch
%{?reqs:Requires: %{reqs}}
%{?provs:Provides: %{provs}}
%{?cfls:Conflicts: %{cfls}}

%description
%{summary}

%files
%defattr(-,root,root,-)
