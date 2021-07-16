%bcond_with symlink
%bcond_with file

Name:		symlinktest-pretrans
Version:	1.0
Release:	%{rel}
Summary:	Testing symlink behavior with pretrans scriptlet
Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

# Details: https://docs.fedoraproject.org/en-US/packaging-guidelines/
#          Directory_Replacement/
%if %{without symlink}
%pretrans -p <lua>
path = "/usr/share/%{name}"
st = posix.stat(path)
if st and st.type == "link" then
  os.remove(path)
end
%endif

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share
mkdir -p $RPM_BUILD_ROOT/usr/lib/%{name}
%if %{with file}
echo foo > $RPM_BUILD_ROOT/usr/lib/%{name}/README
%endif
%if %{with symlink}
ln -s ../lib/%{name} $RPM_BUILD_ROOT/usr/share/%{name}
%else
mkdir -p $RPM_BUILD_ROOT/usr/share/%{name}
%if %{with file}
echo bar > $RPM_BUILD_ROOT/usr/share/%{name}/README
%endif
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/lib/%{name}
/usr/share/%{name}
