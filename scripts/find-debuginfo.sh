#!/bin/sh
#find-debuginfo.sh - automagically generate debug info and file list
#for inclusion in an rpm spec file.

if [ -z "$1" ] ; then BUILDDIR="."
else BUILDDIR=$1
fi

LISTFILE=$BUILDDIR/debugfiles.list
SOURCEFILE=$BUILDDIR/debugsources.list

echo -n > $SOURCEFILE

# Strip ELF binaries
for f in `find $RPM_BUILD_ROOT -type f \( -perm -0100 -or -perm -0010 -or -perm -0001 \) -exec file {} \; | \
	sed -n -e 's/^\(.*\):[ 	]*ELF.*, not stripped/\1/p'`; do
	BASEDIR=`dirname $f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p"`
	OUTPUTDIR=${RPM_BUILD_ROOT}/usr/lib/debug${BASEDIR}
	mkdir -p ${OUTPUTDIR}
	echo extracting debug info from $f
	/usr/lib/rpm/debugedit -b $RPM_BUILD_DIR -d /usr/src/debug -l $SOURCEFILE $f
	eu-strip -f $OUTPUTDIR/`basename $f`.debug $f || :
done

mkdir -p ${RPM_BUILD_ROOT}/usr/src/debug
cat $SOURCEFILE | (cd $RPM_BUILD_DIR; LANG=C sort -z -u | cpio -pd0m ${RPM_BUILD_ROOT}/usr/src/debug)
# stupid cpio creates new directories in mode 0700, fixup
find ${RPM_BUILD_ROOT}/usr/src/debug -type d -print0 | xargs -0 chmod a+rx

find ${RPM_BUILD_ROOT}/usr/lib/debug -type f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p" > $LISTFILE
find ${RPM_BUILD_ROOT}/usr/src/debug -mindepth 1 -maxdepth 1 | sed -n -e "s#^$RPM_BUILD_ROOT#/#p" >> $LISTFILE
