Summary: C library for parsing command line parameters
Name: popt
Version: 1.0
Release: 1
Copyright: LGPL
Group: Utilities/System
Source: ftp://ftp.redhat.com/pub/redhat/code/popt/popt-1.0.tar.gz
BuildRoot: /var/tmp/popt.root

%description
Popt is a C library for pasing command line parameters. It was heavily
influenced by the getopt() and getopt_long() functions, but it allows
more powerfull argument expansion. It can parse arbitrary argv[] style
arrays and automatically set variables based on command line arguments.
It also allows command line arguments to be aliased via configuration
files and includes utility functions for parsing arbitrary strings into
argv[] arrays using shell-like rules. 

%prep
%setup -n popt

%build
make CFLAGS="$RPM_OPT_FLAGS"

%install
make PREFIX=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%attr(0644, root, root) /usr/lib/libpopt.a
%attr(0644, root, root) /usr/include/popt.h
