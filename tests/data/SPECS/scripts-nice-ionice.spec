Name:           scripts-nice-ionice
Version:        1.0
Release:        %{rel}
Summary:        Testing reset of nice/ionice levels on scripts
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%files
%defattr(-,root,root,-)

%pretrans
echo %{name}-%{version}-%{release} PRETRANS $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%pre
echo %{name}-%{version}-%{release} PRE $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%post
echo %{name}-%{version}-%{release} POST $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%preun
echo %{name}-%{version}-%{release} PREUN $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%postun
echo %{name}-%{version}-%{release} POSTUN $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%posttrans
echo %{name}-%{version}-%{release} POSTTRANS $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')

%verifyscript
echo %{name}-%{version}-%{release} VERIFY $*
echo Nice value is $(/bin/nice)
echo IOnice value is $(/usr/bin/ionice | sed -e 's/^unknown:/none:/')
