#!/bin/sh
#find-debuginfo.sh - automagically generate debug info and file list
#for inclusion in an rpm spec file.

LISTFILE=debugfiles.list

# Strip ELF binaries
for f in `find $RPM_BUILD_ROOT -type f \( -perm -0100 -or -perm -0010 -or -perm -0001 \) -exec file {} \; | \
	sed -n -e 's/^\(.*\):[ 	]*ELF.*, not stripped/\1/p'`; do
	BASEDIR=`dirname $f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p"`
	OUTPUTDIR=${RPM_BUILD_ROOT}/usr/lib/debug${BASEDIR}
	mkdir -p ${OUTPUTDIR}
	echo extracting debug info from $f
	striptofile -g -u -o $OUTPUTDIR $f || :
done

find ${RPM_BUILD_ROOT}/usr/lib/debug -type f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p" > $LISTFILE
