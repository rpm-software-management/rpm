#!/bin/sh

#
# Original Author: Tim Mooney (mooney@plains.NoDak.edu)
# 
# This file is distributed under the terms of the GNU General Public License
#
# non-linux-provides is part of RPM, the Red Hat Package Manager.
# non-linux-provides searches a list of directories (based on what OS it's
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

PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/ucb:/usr/bsd
export PATH

#
# The default directories to use if they're not specified as
# arguments 1 and 2, respectively.
#
default_spec_header='/usr/local/lib/rpm/os-base-header.spec';
default_find_provides='/usr/local/lib/rpm/find-provides';

osname=`uname -s`
if test $? -ne 0 || test X$osname = X ; then
	echo "I can't determine what platform this is.  Exiting"
	exit 1
fi

if test X$1 = X ; then
	if test -f $default_spec_header ; then
		spec_header=$default_spec_header
	else
		echo "You must pass me the full path to the partial spec file"
		echo "as my first argument, since this file does not appear in the"
		echo "default location of $default_spec_header"
		echo
		echo "usage: $0 [ /path/to/spec-header ] [ /path/to/find-provides ]"
		echo
		exit 9
	fi
else
	spec_header=$1
	if test ! -f $spec_header ; then
		echo "$spec_header does not exist or is not what I was expecting."
		exit 10
	fi
fi

if test X$2 = X ; then
	if test -f $default_find_provides ; then
		find_provides=$default_find_provides
	else
		echo "You must pass me the full path to the find-provides script as my"
		echo "second argument, since find-provides does not appear in the"
		echo "default location of $default_find_provides"
		echo
		echo "usage: $0 [ /path/to/spec-header ] [ /path/to/find-provides ]"
		echo
		exit 9
	fi
else
	find_provides=$2
	if test ! -f $find_provides ; then
		echo "$find_provides does not exist or is not what I was expecting."
		exit 10
	fi
fi

#
# Set what directories we search for shared libraries and what interpreters
# we look for, based on what OS we're on.
#
case $osname in
	OSF1)
		shlib_dirs='/shlib:/usr/shlib:/usr/dt/lib:/usr/opt'
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/bin/posix'
		interps="sh:csh:ksh:dtksh:wish:tclsh:perl:awk:gawk:nawk:oawk"
		;;
	HP-UX)
		shlib_dirs='/usr/shlib:/usr/dt/lib:/opt'
		shlib_dirs="$shlib_dirs:/usr/bms:/usr/obam:/usr/sam"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/bin/posix'
		interps="sh:csh:ksh:dtksh:wish:tclsh:perl:awk:gawk:nawk:oawk"
		;;
	AIX)
		shlib_dirs='/usr/lib:/usr/ccs/lib:/usr/dt/lib:/usr/lpp:/usr/opt'
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin'
		interps="bsh:sh:csh:ksh:dtksh:wish:tclsh:perl:awk:gawk:nawk:oawk"
		;;
	SunOS)
		shlib_dirs='/etc/lib:/etc/vx:/opt:/usr/lib:/usr/ccs/lib:/usr/dt/lib'
		shlib_dirs="$shlib_dirs:/usr/4lib:/usr/openwin/lib:/usr/snadm/lib"
		shlib_dirs="$shlib_dirs:/usr/ucblib:/usr/xpg4/lib"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/dt/bin:/usr/xpg4/bin'
		interps="bsh:sh:csh:ksh:dtksh:wish:tclsh:perl:awk:gawk:nawk:oawk"
		;;
	IRIX|IRIX64)
		shlib_dirs='/lib:/usr/lib:/usr/lib32:/usr/lib64'
		# Irix always makes me laugh...
		shlib_dirs="$shlib_dirs:/usr/ToolTalk:/usr/xfsm:/usr/SpeedShop"
		shlib_dirs="$shlib_dirs:/usr/sgitcl:/usr/SGImeeting:/usr/pcp/lib"
		shlib_dirs="$shlib_dirs:/usr/Motif-2.1"
		interp_dirs='/bin:/usr/bin:/sbin:/usr/sbin:/usr/dt/bin'
		interps="sh:csh:tcsh:ksh:dtksh:wish:tclsh:perl:perl5:awk:gawk:nawk:oawk"
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

tmp_file=/tmp/shlibs.$$
if test -f $tmp_file ; then
	echo "$tmp_file already exists.  Exiting."
	exit 11
fi

#
# iterate through all the directories in shlib_dirs, looking for shared
# libraries
#
for d in `echo $shlib_dirs | sed -e 's/:/ /g'`
do
	find $d -type f -print 2>/dev/null | $find_provides >> $tmp_file
done

provides=/tmp/provides.$$
if test -f $provides ; then
	echo "$provides already exists.  Exiting."
	exit 11
fi

#
# output the initial part of the spec file
#
cat $spec_header

#
# Output the shared libraries
#
for f in `cat $tmp_file | sort -u`
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

#
# Finish off the spec file we're spitting out.
#
date=`date`
hostname=`uname -n`

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

echo '%prep'
echo '# nothing to do'
echo '%build'
echo '# nothing to do'
echo '%install'
echo '# nothing to do'
echo '%clean'
echo '# nothing to do'
echo '%files'
