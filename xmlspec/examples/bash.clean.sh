echo "Clean script"
rm -rf $RPM_BUILD_DIR/%{name}-%{version}
rm -rf %{buildroot}
