# avoid depending on rpm configuration
%define _datadir /usr/share

Name:           find-lang-test 
Version:        1.0
Release:        1
Summary:        Testing find-lang behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%prep
%setup -c -T

%install
for f in fi de_DE pl ""; do
    mkdir -p $RPM_BUILD_ROOT/%{_datadir}/man/$f/man1
    echo "This is $f language" | gzip > $RPM_BUILD_ROOT/%{_datadir}/man/$f/man1/%{name}.1.gz
done


#echo "This is en language" | gzip > $RPM_BUILD_ROOT/%{_datadir}/man/man1/%{name}.1.gz

mkdir -p  $RPM_BUILD_ROOT/%{_datadir}/%{name}
touch $RPM_BUILD_ROOT/%{_datadir}/%{name}/empty.txt

%find_lang  %{name} --with-man --without-mo %{?langpacks:--generate-subpackages}

%files -f %{name}.lang
%defattr(-,root,root,-)
%{_datadir}/%{name}/empty.txt
%{_datadir}/man/man1/%{name}*
