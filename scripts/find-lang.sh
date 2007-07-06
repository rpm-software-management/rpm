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

# 2004-06-20 Arkadiusz Mi¶kiewicz <arekm@pld-linux.org>
#   * merge PLD changes, kde, all-name (mkochano,pascalek@PLD)
# 1999-10-19 Artur Frysiak <wiget@pld-linux.org>
#   * added support for GNOME help files
#   * start support for KDE help files

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
Additional options:
  --with-gnome		find GNOME help files
  --with-kde		find KDE help files
  --all-name		match all package/domain names
  --without-mo		not find locales files
EOF
exit 1
}

if [ -z "$1" ] ; then usage
elif [ $1 = / ] ; then echo $0: expects non-/ argument for '$1' 1>&2
elif [ ! -d $1 ] ; then
 echo $0: $1: no such directory
 exit 1
else TOP_DIR="`echo $1|sed -e 's:/$::'`"
fi
shift

if [ -z "$1" ] ; then usage
else NAME=$1
fi
shift

GNOME=#
KDE=#
MO=
MO_NAME=$NAME.lang
ALL_NAME=#
NO_ALL_NAME=

while test $# -gt 0 ; do
    case "${1}" in
	--with-gnome )
  		GNOME=
		shift
		;;
	--with-kde )
		KDE=
		shift
		;;
	--without-mo )
		MO=#
		shift
		;;
	--all-name )
		ALL_NAME=
		NO_ALL_NAME=#
		shift
		;;
	* )
		MO_NAME=${1}
		shift
		;;
    esac
done    

find $TOP_DIR -type f -or -type l|sed '
s:'"$TOP_DIR"'::
'"$ALL_NAME$MO"'s:\(.*/share/locale/\)\([^/_]\+\)\(.*\.mo$\):%lang(\2) \1\2\3:
'"$NO_ALL_NAME$MO"'s:\(.*/share/locale/\)\([^/_]\+\)\(.*/'"$NAME"'\.mo$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' > $MO_NAME

find $TOP_DIR -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$GNOME"'s:\(.*/gnome/help/'"$NAME"'$\):%dir \1:
'"$NO_ALL_NAME$GNOME"'s:\(.*/gnome/help/'"$NAME"'/[a-zA-Z0-9.\_\-]/.\+\)::
'"$NO_ALL_NAME$GNOME"'s:\(.*/gnome/help/'"$NAME"'\/\)\([^/_]\+\):%lang(\2) \1\2:
'"$ALL_NAME$GNOME"'s:\(.*/gnome/help/[a-zA-Z0-9.\_\-]\+$\):%dir \1:
'"$ALL_NAME$GNOME"'s:\(.*/gnome/help/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]/.\+\)::
'"$ALL_NAME$GNOME"'s:\(.*/gnome/help/[a-zA-Z0-9.\_\-]\+\/\)\([^/_]\+\):%lang(\2) \1\2:
s:%lang(.*) .*/gnome/help/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]\+/.*::
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find $TOP_DIR -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$KDE"'s:\(.*/doc/kde/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'/\)::
'"$NO_ALL_NAME$KDE"'s:\(.*/doc/kde/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'\)$:%lang(\2) \1\2\3:
'"$ALL_NAME$KDE"'s:\(.*/doc/kde/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+/\)::
'"$ALL_NAME$KDE"'s:\(.*/doc/kde/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

if ! grep -q / $MO_NAME; then
	echo "No translations found for ${NAME} in ${TOP_DIR}"
	exit 1
fi
exit 0
