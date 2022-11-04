Name:           reloc
Version:        1.0
Release:        1
Summary:        Testing relocation behavior
Group:		Testing
License:        GPL
Prefixes:	/opt/bin /opt/etc /opt/lib
BuildArch:	noarch

%description
%{summary}

%install
mkdir -p $RPM_BUILD_ROOT/opt/bin
mkdir -p $RPM_BUILD_ROOT/opt/lib
mkdir -p $RPM_BUILD_ROOT/opt/etc
touch $RPM_BUILD_ROOT/opt/bin/typo
touch $RPM_BUILD_ROOT/opt/lib/notlib
touch $RPM_BUILD_ROOT/opt/etc/conf

%pre -p <lua>
for i, p in ipairs(RPM_INSTALL_PREFIX) do
   print(i..": "..p)
end

%post
echo 0: $RPM_INSTALL_PREFIX0
echo 1: $RPM_INSTALL_PREFIX1
echo 2: $RPM_INSTALL_PREFIX2

%files
%defattr(-,root,root,-)
/opt
