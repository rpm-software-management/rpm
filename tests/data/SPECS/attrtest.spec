Name:           attrtest 
Version:        1.0
Release:        1
Summary:        Testing file attr behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT

for x in a b c d e f g h i j; do
    mkdir -p $RPM_BUILD_ROOT/${x}
    mkdir -p $RPM_BUILD_ROOT/${x}/dir
    echo "${x}" > $RPM_BUILD_ROOT/${x}/file
    chmod 700 $RPM_BUILD_ROOT/${x}/dir
    chmod 400 $RPM_BUILD_ROOT/${x}/file
done

%files -n %{name}
/a/dir
/a/file

%attr(-,daemon,adm) /b/dir
%attr(-,daemon,adm) /b/file

%attr(750,-,adm) /c/dir
%attr(640,daemon,-) /c/file

%attr(751,daemon,bin) /d/dir
%attr(644,bin,daemon) /d/file

%defattr(-,foo,bar)
/e/dir
/e/file

%defattr(-,bar,foo)
%attr(770,-,-) /f/dir
%attr(660,-,-) /f/file

%defattr(-,bar,foo)
%attr(-,adm,-) /g/dir
%attr(-,-,adm) /g/file

%defattr(644,foo,bar,755)
/h/dir
/h/file

%defattr(4755,root,root,750)
%attr(-,adm,-) /i/dir
%attr(-,-,adm) /i/file

%defattr(640,zoot,zoot,750)
%attr(777,-,-) /j/dir
%attr(222,-,-) /j/file
