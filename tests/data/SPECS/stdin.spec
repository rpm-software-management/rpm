Name: stdin
Version: 1.0
Release: 1
Summary: libtool weirdness test-case
License: Public Domain

%prep
# func_lalib_unsafe_p in libtool does this
echo Fo > bar
exec 5<&0 < bar
exec 0<&2 2<&-

%files
