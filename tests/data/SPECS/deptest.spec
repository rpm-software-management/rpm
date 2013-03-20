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
%{?obs:Obsoletes: %{obs}}

%description
%{summary}

%install
mkdir -p %{buildroot}/opt/
echo FOO > %{buildroot}/opt/bar

%files
%defattr(-,root,root,-)
/opt/bar
