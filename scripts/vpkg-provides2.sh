#!/bin/sh


PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd
export PATH

IGNORE_DIRS='@'

date=`date`
hostname=`uname -n`
osname=`uname -s`

# programs we run

#find_provides=/usr/local/lib/rpm/find-provides
#find_requires=/usr/local/lib/rpm/find-requires
find_provides=/devel/kestes/vendorc/tools/solaris.prov
find_requires=/devel/kestes/vendorc/tools/solaris.req
u_pkg=/devel/kestes/vendorc/rpm/scripts/u-pkg.sh 

# where we write output
spec_filedir=/tmp
provides_tmp=/tmp/provides.$$
requires_tmp=/tmp/requires.$$


for pkg in `$u_pkg list_all_packages`
do

# find OS pkg information

spec_filename=$spec_filedir/$pkg

veryify_cmd=`$u_pkg print_cmd package_version $pkg | sed -e "s/\\$1/$pkg/" `

pkg_version=`$u_pkg package_version $pkg `


# find all the dependencies

$u_pkg list_all_files_in_package $pkg | egrep -v \'$IGNORE_DIRS\' | \
	$find_provides | sed -e 's/^/Provides: /' > $provides_tmp

$u_pkg list_all_files_in_package $pkg | egrep -v \'$IGNORE_DIRS\' | \
	$find_requires | sed -e 's/^/Requires: /' > $requires_tmp

# create the spec file

rm -f $spec_filename

echo >> $spec_filename

cat $provides_tmp | sort -u >> $spec_filename

echo >> $spec_filename

cat $requires_tmp | sort -u >> $spec_filename

echo >> $spec_filename


# Output the rest of the spec file.  It is a template stored in this
# here file.


cat >> $spec_filename <<_EIEIO_
Name: vpkg-$pkg
Version: $pkg_version

%description
This is a virtual RPM package.  It contains no actual files.  It uses the
\`Provides' token from RPM 3.x and later to list many of the shared libraries
and interpreters that are part of the base operating system and associated
subsets for $osname.

This virtual package was constructed based on the vendor/system software
installed on the $osname machine named $hostname, as of the date
$date.  It is intended to supply dependency 
information about the OS package: $pkg, version: $pkg_version,


%prep
# nothing to do

%build
# nothing to do

%install
# nothing to do

%clean
# nothing to do


%verifyscript

PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd:/usr/local/bin
export PATH

expected_version='$pkg_version'
current_version=\`$veryify_cmd\`

if [ \$expected_version -ne \$current_version ]; then
	echo "RPM virtual package does not match OS pkg: $pkg" >&2
	echo "installed packge version: \$current_verion" >&2
	echo "expected package version: \$expected_version" >&2
	exit 9
fi

%files

_EIEIO_

done  

