%{!?filetype: %global filetype file}
%{?fixit: %global havepretrans 1}

Name:		replacetest%{?sub:-%{sub}}
Version:	%{ver}
Release:	1
Summary:	Testing file replacement behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/opt
case %{filetype} in
file)
    echo "%{filedata}" > $RPM_BUILD_ROOT/opt/foo
    ;;
link)
    ln -s "%{filedata}" $RPM_BUILD_ROOT/opt/foo
    ;;
dir)
    mkdir -p $RPM_BUILD_ROOT/opt/foo
    ;;
datadir)
    mkdir -p $RPM_BUILD_ROOT/opt/foo
    echo WOOT > $RPM_BUILD_ROOT/opt/foo/%{filedata}
    ;;
esac
mkdir -p $RPM_BUILD_ROOT/opt/zoo
echo FOO > $RPM_BUILD_ROOT/opt/goo

%clean
rm -rf $RPM_BUILD_ROOT

%if 0%{?havepretrans}
%pretrans -p <lua>
%{fixit}
%endif

%files
%defattr(-,root,root,-)
/opt/*
