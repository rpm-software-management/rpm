Name: suicidal
Version: 1
Release: %{rel}
License: GPL
Group: Testing
Summary: Testing suicidal package behavior
BuildArch: noarch

%description

%build
mkdir -p %{buildroot}/opt
echo shoot > %{buildroot}/opt/foot

%pre -p <lua>
os.remove('/opt/foot')

%files
/opt/foot
