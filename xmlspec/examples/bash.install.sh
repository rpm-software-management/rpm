echo "Install script"
make DESTDIR=%{buildroot} install
cd %{buildroot}/bin
ln -sf bash sh
rm -rf %{buildroot}/usr/info/dir

