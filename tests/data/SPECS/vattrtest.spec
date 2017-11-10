Name:           vattrtest
Version:        1.0
Release:        1
Summary:        Testing virtual file attribute behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/opt/%{name}
for x in a c cn d g l m r; do
    echo ${x} > $RPM_BUILD_ROOT/opt/%{name}/${x}
done

%files
%dir /opt/%{name}
%artifact /opt/%{name}/a
%config /opt/%{name}/c
%config(noreplace) /opt/%{name}/cn
%doc /opt/%{name}/d
%ghost /opt/%{name}/g
%license /opt/%{name}/l
%missingok /opt/%{name}/m
%readme /opt/%{name}/r
