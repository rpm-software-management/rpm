#!/bin/sh

#
# Original Author: Tim Mooney (mooney@plains.NoDak.edu)
# Improvements by: Ken Estes <kestes@staff.mail.com>
# 
# This file is distributed under the terms of the GNU General Public License
#
# vpkg-provides.sh is part of RPM, the Red Hat Package Manager.
# vpkg-provides.sh searches a list of directories (based on what OS it's
# being executed on) for shared libraries and interpreters that have been
# installed by some packaging system other than RPM.  It then generates a
# spec file that can be used to build a "virtual package" that provides all
# of these things without actually installing any files.  This makes it much
# easier to use RPM on non-Linux systems.
#
# Comments: This script is a quick hack.  A better solution is to use the
# vendor's package management commands to actually query what's installed, and
# build one or more spec files based on that.  This is something
# I intend to write, probably in perl, but the need for something like this
# first effort was great, so I didn't want to wait until the better solution
# was done.

# you will need to create a spec_header for the virtual package.  This
# header will provide such specfile information as:
#
#  Summary: 
#  Name: 
#  Version: 
#  Release: 
#  Copyright: 
#  Group: 
#  Source: 


usage= "usage: $0 [--spec_header '/path/to/os-base-header.spec'] \n"
usage= "$usage\t[--find_provides '/path/to/find-provides']\n"
usage= "$usage\t[--shlib_dirs 'dirs:which:contain:shared:libs']\n"
usage= "$usage\t[--ignore_dirs 'egrep|pattern|of|paths|to|ignore']\n"

# these two should be unnessary as the regular dependency analysis
# should take care of interpreters as well as shared libraries.

usage= "$usage\t[--interp_dirs 'dirs:which:contain:interpreters']\n"
usage= "$usage\t[--interps 'files:to:assume:are:installed']\n"


# this command may not be portable to all OS's, does something else
# work? can this be set in the case $osname statement?

sum_cmd="xargs cksum"

date=`date`
hostname=`uname -n`

# if some subdirectories of the system directories needs to be ignored
# (eg /usr/local is a subdirectory of /usr but should not be part of
# the virtual package) then call this script with IGNORE_DIRS set to a
# vaild egrep pattern which discribes the directories to ignored.

PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd
export PATH


#
# The (OS independent) default values.
#
spec_header='/usr/lib/rpm/os-base-header.spec';
interps="sh:csh:ksh:dtksh:wish:tclsh:perl:awk:gawk:nawk:oawk"
find_provides='/usr/lib/rpm/find-provides';
ignore_dirs="."


osname=`uname -s`
if test $? -ne 0 || test X$osname = X ; then
	echo "I can't determine what platform this is.  Exiting"
	exit 1
fi


#
# Set OS dependent defaults
#
case $osname in
	OSF1)
		shlib_dirs='/shlib:/usr/shlib:/usr/dt/lib:/usr/opt'
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/bin/posix'
		;;
	HP-UX)
		shlib_dirs='/usr/shlib:/usr/dt/lib:/opt'
		shlib_dirs="$shlib_dirs:/usr/bms:/usr/obam:/usr/sam"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/bin/posix'
		;;
	AIX)
		shlib_dirs='/usr/lib:/usr/ccs/lib:/usr/dt/lib:/usr/lpp:/usr/opt'
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin'
		;;
	SunOS)
		shlib_dirs='/etc/lib:/etc/vx:/opt:/usr/lib:/usr/ccs/lib:/usr/dt/lib'
		shlib_dirs="$shlib_dirs:/usr/4lib:/usr/openwin/lib:/usr/snadm/lib"
		shlib_dirs="$shlib_dirs:/usr/ucblib:/usr/xpg4/lib"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/xpg4/bin'
		;;
	IRIX|IRIX64)
		shlib_dirs='/lib:/usr/lib:/usr/lib32:/usr/lib64'
		# Irix always makes me laugh...
		shlib_dirs="$shlib_dirs:/usr/ToolTalk:/usr/xfsm:/usr/SpeedShop"
		shlib_dirs="$shlib_dirs:/usr/sgitcl:/usr/SGImeeting:/usr/pcp/lib"
		shlib_dirs="$shlib_dirs:/usr/Motif-2.1"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/sbin:/usr/dt/bin'
		;;
	*)
		echo "I'm sorry.  I haven't been configured yet to work on $osname."
		echo "Please poke around your system and try figure out what directories"
		echo "I should be searching for shared libraries.  Once you have this"
		echo "information, email it to rpm-list@redhat.com, so that your OS"
		echo "will be supported by some future version of this script."
		echo ""
		echo "Thanks!"
		echo
		exit 2
		;;
esac


# allow the user to change defaults with the command line arguments.

# Loop over all args

while :
do

# Break out if there are no more args
	case $# in
	0)
		break
		;;
	esac

# Get the first arg, and shuffle
	option=$1
	shift

# Make all options have two hyphens
	orig_option=$option	# Save original for error messages
	case $option in
	--*) ;;
	-*) option=-$option ;;
	esac


	case $option in
	--spec_header)
		spec_header=$1
		shift
		;;
	--ignore_dirs)
		ignore_dirs=$1
		shift
		;;
	--find_provides)
		find_provides=$1
		shift
		;;
	--shlib_dirs)
		shlib_dirs=$1
		shift
		;;
	--interp_dirs)
		interp_dirs=$1
		shift
		;;
	--interps)
		interps=$1
		shift
		;;
	--help)
		echo $usage
		exit 0
		;;
	*)
		echo "$0: Unrecognized option: \"$orig_option\"; use --help for usage." >&2
		exit 1
		;;
	esac
done


# consistancy checks on the arguments

if [ ! -f $spec_header ]; then
	echo "You must pass me the full path to the partial spec file"
	echo "as my first argument, since this file does not appear in the"
	echo "default location of $default_spec_header"
	echo
	echo $usage
	echo
	exit 9
fi


if [ ! -f $find_provides ]; then
	echo "You must pass me the full path to the find-provides script as my"
	echo "second argument, since find-provides does not appear in the"
	echo "default location of $default_find_provides"
	echo
	echo $usage
	echo
	exit 9
fi



provides_tmp=/tmp/provides.$$
if test -f $provides_tmp ; then
	echo "$provides_tmp already exists.  Exiting."
	exit 11
fi

#
# iterate through all the directories in shlib_dirs, looking for shared
# libraries
#
for d in `echo $shlib_dirs | sed -e 's/:/ /g'`
do
	find $d -type f -print 2>/dev/null | egrep -v \'$IGNORE_DIRS\' | $find_provides >> $provides_tmp
done

sum_tmp=/tmp/sum.$$
if test -f $sum_tmp ; then
	echo "$sum_tmp already exists.  Exiting."
	exit 11
fi

#
# iterate through all the directories in shlib_dirs, record the sum
#
for d in `echo $shlib_dirs | sed -e 's/:/ /g'`
do
	find $d -type f -print 2>/dev/null | egrep -v \'$IGNORE_DIRS\' | $sum_cmd >> $sum_tmp
done


#
# output the initial part of the spec file
#
cat $spec_header

#
# Output the shared libraries
#
{
    for f in `cat $provides_tmp | sort -u`
    do
	echo "Provides: $f"
    done

#
# Output the available shell interpreters
#
    for d in `echo $interp_dirs | sed -e 's/:/ /g'`
    do
	for f in `echo $interps | sed -e 's/:/ /g'`
	do
		if test -f $d/$f ; then
			echo "Provides: $d/$f"
		fi
	done
    done
} | sed -e 's/%/%%/g'

#
# Output the discription of the spec file
#

cat <<_EIEIO_


%description
This is a virtual RPM package.  It contains no actual files.  It uses the
\`Provides' token from RPM 3.x and later to list many of the shared libraries
and interpreters that are part of the base operating system and associated
subsets for $osname.

This virtual package was constructed based on the vendor/system software
installed on the $osname machine named $hostname, as of the date
$date.

_EIEIO_

#
# Output the build sections of the spec file
#
echo '%prep'
echo '# nothing to do'
echo '%build'
echo '# nothing to do'
echo '%install'
echo '# nothing to do'
echo '%clean'
echo '# nothing to do'

#
# Output the verify section of the spec file
#

cat <<_EIEIO_

%verifyscript

PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd
export PATH

sum_current_tmp=/tmp/rpm.sum.current.\$\$
if test -f \$sum_current_tmp ; then
	echo "\$sum_current_tmp already exists.  Exiting."
	exit 11
fi

sum_package_tmp=/tmp/rpm.sum.package.\$\$
if test -f \$sum_package_tmp ; then
	echo "\$sum_package_tmp already exists.  Exiting."
	exit 11
fi

for d in `echo $shlib_dirs | sed -e 's/:/ /g'`
do
	find \$d -type f -print 2>/dev/null | egrep -v \'$IGNORE_DIRS\' | $sum_cmd >> \$sum_current_tmp
done

cat >\$sum_package_tmp <<_EOF_
_EIEIO_

# the contents of the temporary file are hardcoded into the verify
# script so that the file can be reproduced at verification time.

cat $sum_tmp | sed -e 's/%/%%/g'

cat <<_EIEIO_
_EOF_


cmp \$sum_package_tmp \$sum_current_tmp 

if [ $? -ne 0 ]; then
	echo "Differences found by: cmp \$sum_package_tmp \$sum_current_tmp"
	exit \$?
fi

_EIEIO_

#
# Output the files section of the spec file
#

echo '%files'
echo '# no files in a virtual package'
