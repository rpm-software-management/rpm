#!/bin/bash
#findlang - automagically generate list of language specific files
#for inclusion in an rpm spec file.
#This does assume that the *.mo files are under .../locale/...
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
Additional options:
  --with-gnome		find GNOME help files
  --with-mate		find MATE help files
  --with-kde		find KDE help files
  --with-qt		find Qt translation files
  --with-html		find HTML files
  --with-man		find localized man pages
  --all-name		match all package/domain names
  --without-mo		do not find locale files
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
else NAMES[0]=$1
fi
shift

GNOME=#
MATE=#
KDE=#
QT=#
MAN=#
HTML=#
MO=
MO_NAME=${NAMES[0]}.lang
ALL_NAME=#
NO_ALL_NAME=
while test $# -gt 0 ; do
    case "${1}" in
	--with-gnome )
  		GNOME=
		shift
		;;
	--with-mate )
		MATE=
		shift
		;;
	--with-kde )
		KDE=
		shift
		;;
	--with-qt )
		QT=
		shift
		;;
	--with-man )
		MAN=
		shift
		;;
	--with-html )
		HTML=
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
		if [ $MO_NAME != ${NAMES[$#]}.lang ]; then
		    NAMES[${#NAMES[@]}]=$MO_NAME
		fi
		MO_NAME=${1}
		shift
		;;
    esac
done    

if [ -f $MO_NAME ]; then
    rm $MO_NAME
fi

for NAME in ${NAMES[@]}; do

find "$TOP_DIR" -type f -o -type l|sed '
s:'"$TOP_DIR"'::
'"$ALL_NAME$MO"'s:\(.*/locale/\)\([^/_]\+\)\(.*\.mo$\):%lang(\2) \1\2\3:
'"$NO_ALL_NAME$MO"'s:\(.*/locale/\)\([^/_]\+\)\(.*/'"$NAME"'\.mo$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$GNOME"'s:\(.*/share/help/\)\([^/_]\+\)\([^/]*\)\(/'"$NAME"'\)$:%lang(\2) %doc \1\2\3\4/:
'"$ALL_NAME$GNOME"'s:\(.*/share/help/\)\([^/_]\+\)\([^/]*\)\(/[a-zA-Z0-9.\_\-]\+\)$:%lang(\2) %doc \1\2\3\4/:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type d|sed '
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

find "$TOP_DIR" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$GNOME"'s:\(.*/omf/'"$NAME"'$\):%dir \1:
'"$ALL_NAME$GNOME"'s:\(.*/omf/[a-zA-Z0-9.\_\-]\+$\):%dir \1:
s:^\([^%].*\)::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type f|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$GNOME"'s:\(.*/omf/'"$NAME"'/'"$NAME"'-\([^/.]\+\)\.omf\):%lang(\2) \1:
'"$ALL_NAME$GNOME"'s:\(.*/omf/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]\+-\([^/.]\+\)\.omf\):%lang(\2) \1:
s:^[^%].*::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find $TOP_DIR -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$MATE"'s:\(.*/mate/help/'"$NAME"'$\):%dir \1:
'"$NO_ALL_NAME$MATE"'s:\(.*/mate/help/'"$NAME"'/[a-zA-Z0-9.\_\-]/.\+\)::
'"$NO_ALL_NAME$MATE"'s:\(.*/mate/help/'"$NAME"'\/\)\([^/_]\+\):%lang(\2) \1\2:
'"$ALL_NAME$MATE"'s:\(.*/mate/help/[a-zA-Z0-9.\_\-]\+$\):%dir \1:
'"$ALL_NAME$MATE"'s:\(.*/mate/help/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]/.\+\)::
'"$ALL_NAME$MATE"'s:\(.*/mate/help/[a-zA-Z0-9.\_\-]\+\/\)\([^/_]\+\):%lang(\2) \1\2:
s:%lang(.*) .*/mate/help/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]\+/.*::
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$MATE"'s:\(.*/omf/'"$NAME"'$\):%dir \1:
'"$ALL_NAME$MATE"'s:\(.*/omf/[a-zA-Z0-9.\_\-]\+$\):%dir \1:
s:^\([^%].*\)::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type f|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$MATE"'s:\(.*/omf/'"$NAME"'/'"$NAME"'-\([^/.]\+\)\.omf\):%lang(\2) \1:
'"$ALL_NAME$MATE"'s:\(.*/omf/[a-zA-Z0-9.\_\-]\+/[a-zA-Z0-9.\_\-]\+-\([^/.]\+\)\.omf\):%lang(\2) \1:
s:^[^%].*::
s:%lang(C) ::
/^$/d' >> $MO_NAME

KDE3_HTML=`kde-config --expandvars --install html 2>/dev/null`
if [ x"$KDE3_HTML" != x -a -d "$TOP_DIR$KDE3_HTML" ]; then
find "$TOP_DIR$KDE3_HTML" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'/\)::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'\)$:%lang(\2) \1\2\3:
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+/\)::
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME
fi

KDE4_HTML=`kde4-config --expandvars --install html 2>/dev/null`
if [ x"$KDE4_HTML" != x -a -d "$TOP_DIR$KDE4_HTML" ]; then
find "$TOP_DIR$KDE4_HTML" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'/\)::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'\)$:%lang(\2) \1\2\3:
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+/\)::
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME
fi

KF5_HTML=`kf5-config --expandvars --install html 2>/dev/null`
if [ x"$KF5_HTML" != x -a -d "$TOP_DIR$KF5_HTML" ]; then
find "$TOP_DIR$KF5_HTML" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'/\)::
'"$NO_ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'\)$:%lang(\2) \1\2\3:
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+/\)::
'"$ALL_NAME$KDE"'s:\(.*/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME
fi

find "$TOP_DIR" -type d|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$HTML"'s:\(.*/doc/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'/\)::
'"$NO_ALL_NAME$HTML"'s:\(.*/doc/HTML/\)\([^/_]\+\)\(.*/'"$NAME"'\)$:%lang(\2) \1\2\3:
'"$ALL_NAME$HTML"'s:\(.*/doc/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+/\)::
'"$ALL_NAME$HTML"'s:\(.*/doc/HTML/\)\([^/_]\+\)\(.*/[a-zA-Z0-9.\_\-]\+$\):%lang(\2) \1\2\3:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type f -o -type l|sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$QT"'s:\(.*/'"$NAME"'_\([a-zA-Z]\{2\}\([_@].*\)\?\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/\([a-zA-Z]\{2\}[_@].*\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/\([a-zA-Z]\{2\}\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/[^/_]\+_\([a-zA-Z]\{2\}[_@].*\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/[^/_]\+_\([a-zA-Z]\{2\}\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/[^/]\+_\([a-zA-Z]\{2\}[_@].*\)\.qm$\):%lang(\2) \1:
'"$ALL_NAME$QT"'s:^\([^%].*/[^/]\+_\([a-zA-Z]\{2\}\)\.qm$\):%lang(\2) \1:
s:^[^%].*::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type d|sed '
s:'"$TOP_DIR"'::
'"$ALL_NAME$MAN"'s:\(.*/man/\([^/_]\+\).*/man[a-z0-9]\+/\)::
'"$ALL_NAME$MAN"'s:\(.*/man/\([^/_]\+\).*/man[a-z0-9]\+$\):%lang(\2) \1*:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

find "$TOP_DIR" -type f -o -type l|sed -r 's/\.(bz2|gz|xz|lzma|Z)$//g' | sed '
s:'"$TOP_DIR"'::
'"$NO_ALL_NAME$MAN"'s:\(.*/man/\([^/_]\+\).*/man[a-z0-9]\+/'"$NAME"'\.[a-z0-9].*\):%lang(\2) \1*:
s:^\([^%].*\)::
s:%lang(C) ::
/^$/d' >> $MO_NAME

done # for NAME in ${NAMES[@]}

if ! grep -q / $MO_NAME; then
	echo "No translations found for ${NAME} in ${TOP_DIR}"
	exit 1
fi
exit 0
