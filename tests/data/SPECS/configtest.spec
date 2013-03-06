# avoid depending on rpm configuration
%define _sysconfdir /etc

%{!?filetype: %global filetype file}

Name:		configtest%{?sub:-%{sub}}
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
case %{filetype} in
file)
    echo "%{filedata}" > $RPM_BUILD_ROOT/%{_sysconfdir}/my.conf
    ;;
link)
    ln -s "%{filedata}" $RPM_BUILD_ROOT/%{_sysconfdir}/my.conf
    ;;
dir)
    mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/my.conf
    ;;
esac

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{?fileattr} %{!?noconfig:%config%{?noreplace:(noreplace)}} %{_sysconfdir}/my.conf
