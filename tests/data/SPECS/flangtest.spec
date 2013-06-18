# avoid depending on rpm configuration
%define _datadir /usr/share

Name:           flangtest 
Version:        1.0
Release:        1
Summary:        Testing file lang behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p  $RPM_BUILD_ROOT/%{_datadir}/%{name}
for f in fi de en pl none; do
    echo "This is $f language" > $RPM_BUILD_ROOT/%{_datadir}/%{name}/$f.txt
done
touch $RPM_BUILD_ROOT/%{_datadir}/%{name}/empty.txt

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%lang(fi) %{_datadir}/%{name}/fi.txt
%lang(de) %{_datadir}/%{name}/de.txt
%lang(en) %{_datadir}/%{name}/en.txt
%lang(pl) %{_datadir}/%{name}/pl.txt
%{_datadir}/%{name}/none.txt
%{_datadir}/%{name}/empty.txt
