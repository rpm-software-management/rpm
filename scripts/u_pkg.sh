#!/bin/sh

# a universal interface to Unix OS package managment systems

# This script is not finished.  It is a bunch of ideas for creating a
# universal package manager using the OS package manager.  I wish to
# only use tools which are installed in the OS by default and be
# portable to all OS package managers.


PATH="/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd:$PATH"
export PATH



osname=`uname -s`
if test $? -ne 0 || test X$osname = X ; then
	echo "I can't determine what platform this is.  Exiting"
	exit 1
fi


#
# Set OS dependent defaults
#

# note: that the "package name" which are returned by this script
# should always include the version and release number.

case $osname in
	Linux)
		check_all_packages='rpm -Va'
		list_all_packages='rpm -qa'
		list_all_files='rpm -qla'
		list_all_files_in_package='rpm -ql $1'
		full_package_name='rpm -q $1'
		query_file='rpm -qf $1'
		;;
	SunOS)
		check_all_packages='/usr/sbin/pkgchk -n'
		list_all_files='/usr/sbin/pkgchk -l | /bin/egrep Pathname | /bin/awk "{print \$2}" '
		list_all_files_in_package='/usr/sbin/pkgchk -l $1 | /bin/egrep Pathname | /bin/awk "{print \$2}" '
		list_all_packages='/usr/bin/pkginfo -x | /bin/sed -e "/^[a-zA-Z]/ { N; /^\\n\$/d; s/ .*$//; }" '
		package_version='/usr/bin/pkginfo -x $1 | egrep -v "^[a-zA-Z]" | sed -e "s/(.*)//; s/\ \ *//" '
		query_file='/usr/sbin/pkgchk -l -p $1 | /bin/egrep -v "^[a-zA-Z]" | xargs /usr/bin/pkginfo -x | /bin/sed -e "/^[a-zA-Z]/ { N; /^\\n\$/d; s/\ .*\\n.*//; }" '
		;;
	OSF1)
		;;
	HP-UX)
		;;
	AIX)
		;;
	IRIX|IRIX64)
		;;
	*)
		echo "I haven't been configured yet to work on $osname."
		echo "email it to rpm-list@redhat.com, so that your OS"
		echo "will be supported by some future version of this script."
		echo ""
		echo "Thanks!"
		echo
		exit 2
		;;
esac



option=$1
shift

# I would like to have the second $ actually interpolate so I could
# drop the second eval.  Anyone know how to do this?

if [ $option = 'print_cmd' ]; then
	option=$1
	shift
	eval echo $"$option"
	exit 0
fi

eval eval $"$option"



