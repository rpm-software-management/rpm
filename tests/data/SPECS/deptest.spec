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
%{?recs:Recommends: %{recs}}
%{?sugs:Suggests: %{sugs}}
%{?sups:Supplements: %{sups}}
%{?ens:Enhances: %{ens}}
%{?ord:OrderWithRequires: %{ord}}
%{?buildreqs:BuildRequires: %{buildreqs}}
%{?buildcfls:BuildConflicts: %{buildcfls}}

%description
%{summary}

%install
mkdir -p %{buildroot}/opt/
echo FOO > %{buildroot}/opt/bar

%files
%defattr(-,root,root,-)
/opt/bar
