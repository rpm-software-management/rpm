Name: hello
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Simple rpm demonstration.

%sourcelist
hello-1.0 %tar.gz
https://example.com/hello-addon ! tar\.gz

%patchlist
hello-1.0-m<o>dernize&renew.patch
http://example.com/hello-1.0-inst(a);;.patch

%description
Simple rpm demonstration.

%prep
for f in %{sources} ; do
    echo "$f"
done
for f in %{patches} ; do
    echo "$f"
done

%files
%doc	FAQ
/usr/local/bin/hello

