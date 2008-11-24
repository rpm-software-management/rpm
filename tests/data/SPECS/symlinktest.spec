%bcond_with symlink

Name:		symlinktest
Version:	1.0
Release:	%{rel}
Summary:	Testing symlink behavior
Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share
%if %{with symlink}
mkdir -p $RPM_BUILD_ROOT/usr/lib/%{name}
echo %{name} > $RPM_BUILD_ROOT/usr/lib/%{name}/README
ln -s ../lib/%{name} $RPM_BUILD_ROOT/usr/share/%{name}
%else
mkdir -p $RPM_BUILD_ROOT/usr/share/%{name}
echo %{name} > $RPM_BUILD_ROOT/usr/share/%{name}/README
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%if %{with symlink}
/usr/lib/%{name}
%endif
/usr/share/%{name}
