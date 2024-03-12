Name:		deptest-%{pkg}
Version:	1.0
Release:	1
Summary:	Testing dependency behavior

Group:		Testing
License:	GPL
BuildArch:	noarch
%{?reqs:Requires%{?reqflags:(%{reqflags})}: %{reqs}}
%{?provs:Provides: %{provs}}
%{?cfls:Conflicts: %{cfls}}
%{?obs:Obsoletes: %{obs}}
%{?recs:Recommends%{?recflags:(%{recflags})}: %{recs}}
%{?sugs:Suggests%{?sugflags:(%{sugflags})}: %{sugs}}
%{?sups:Supplements%{?supflags:(%{supflags})}: %{sups}}
%{?ens:Enhances%{?ensflags:(%{ensflags})}: %{ens}}
%{?ord:OrderWithRequires%{?ordflags:(%{ordflags})}: %{ord}}
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
