#!/bin/sh
#findlang - automagically generate list of language specific files
#for inclusion in an rpm spec file.
#This does assume that the *.mo files are under .../share/locale/...
#Run with no arguments gets a usage message.

#findlang is copyright (c) 1998 by W. L. Estes <wlestes@uncg.edu>

#Redistribution and use of this software are hereby permitted for any
#purpose as long as this notice and the above copyright notice remain
#in tact and are included with any redistribution of this file or any
#work based on this file.

usage () {
cat <<EOF

Usage: $0 TOP_DIR PACKAGE_NAME [prefix]

where TOP_DIR is
the top of the tree containing the files to be processed--should be
\$RPM_BUILD_ROOT usually. TOP_DIR gets sed'd out of the output list.
PACKAGE_NAME is the %{name} of the package. This should also be
the basename of the .mo files.  the output is written to
PACKAGE_NAME.lang unless \$3 is given in which case output is written
to \$3.

EOF
exit 1
}

if [ -z "$1" ] ; then usage
elif [ $1 = / ] ; then echo $0: expects non-/ argument for '$1' 1>&2
elif [ ! -d $1 ] ; then
echo $0: $1: no such directory
exit 1
else TOP_DIR=$1
fi

if [ -z "$2" ] ; then usage
else NAME=$2
fi

MO_NAME=${3:-$NAME.lang}

find $TOP_DIR -name $NAME.mo|sed '
1i\
%defattr (644, root, root, 755)
s:'"$TOP_DIR"'::
s:\(.*/share/locale/\)\([^/_]\+\):%lang(\2) \1\2:
' > $MO_NAME
