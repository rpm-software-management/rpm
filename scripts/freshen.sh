#!/bin/sh

# Traditionally, and to some extent still, rpm --freshen upgraded
# packages that matched by RPMTAG_NAME, not RPMTAG_PROVIDENAME.
#
# This freshen.sh script illustrates how to revert to the "traditional"
# behavior for doing, say,
#	rpm -Fvh kernel-bigmem*.rpm
# so that only kernel-bigmem packages are upgraded, rather than
# upgrading (i.e. erasing) every kernel package that contains
#	Provides: kernel = V-R
#
# Copy the freshen.sh script to /usr/lib/rpm, and add the following
# lines to /etc/popt to enable the behavior:
#	rpm alias -F		--freshen
#	rpm exec --freshen	/usr/lib/rpm/freshen.sh
#	

dbg=	#echo	# Do "dbg=echo" for debugging
#set -x
#echo "args: $*"

# Invoke rpmi from the same directory as freshen.sh.
rpmi="`dirname $0`/rpmi"
rpme="`dirname $0`/rpme"
rpmq="`dirname $0`/rpmq"

# Parse out any options and add to new arglist.
# Note: this fails for options with arguments,
# and doesn't detect multiple -i/-e/-U/-F options either.
opts=""
while [ $# -gt 0 ]; do
    case $1 in
    -*) opt="$1"
	opts="$opts $opt" && shift
	[ "$opt" = "--" ] && break
	;;
    *)	opts="$opts --" && break
	;;
    esac
done
#echo "opts: $opts"

# $opts has the options with final '--', $* has the package files

# If no remaining options, just invoke rpm (which will fail).
[ $# = 0 ] && $dbg exec $rpmi -F $opts

# Split remaining args into erase/install/upgrade invocations
iargs=
eargs=
Fargs=
for fn in $*; do
    # If not a file, just pass to freshen.
    [ ! -f $fn ] && Fargs="$Fargs $fn" && continue

    # For all occurences of identically named packages installed ...
    N="`$rpmq -qp --qf '%{NAME}' $fn`"
    NVR="`$rpmq -qa $N`"

    # ... special case kernel packages, ignore packages not installed.
    case $N in
    kernel*)
	# ... if none installed, skip thi kernel package.
	[ "$NVR" = "" ] && continue

	# ... else install new package before erasing old package(s).
	iargs="$iargs $fn"
	eargs="$eargs $NVR"
	;;
    *) Fargs="$Fargs $fn";;	
    esac

done

set -e		# Exit on any error from here on out.

# Install before erase to insure deps are provided.
[ "$iargs" != "" ] && $dbg $rpmi -i $opts $iargs
[ "$eargs" != "" ] && $dbg $rpme -e $opts $eargs
# Other, non-kernel, files passed to --freshen as always.
[ "$Fargs" != "" ] && $dbg $rpmi -F $opts $Fargs
