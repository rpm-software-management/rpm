#!/bin/sh
#find-debuginfo.sh - automagically generate debug info and file list
#for inclusion in an rpm spec file.

LISTFILE=debugfiles.list
SOURCEFILE=debugsources.list

touch .debug_saved_mode
echo -n > $SOURCEFILE

# Strip ELF binaries
for f in `find $RPM_BUILD_ROOT -type f \( -perm -0100 -or -perm -0010 -or -perm -0001 \) -exec file {} \; | \
	sed -n -e 's/^\(.*\):[ 	]*ELF.*, not stripped/\1/p'`; do
	BASEDIR=`dirname $f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p"`
	OUTPUTDIR=${RPM_BUILD_ROOT}/usr/lib/debug${BASEDIR}
	mkdir -p ${OUTPUTDIR}
	echo extracting debug info from $f
	#save old mode
	chmod --reference=$f .debug_saved_mode
	#make sure we have write perms
	chmod u+w $f
	/usr/lib/rpm/debugedit -b $RPM_BUILD_DIR -d /usr/src/debug -l $SOURCEFILE $f
	chmod --reference=.debug_saved_mode $f
	/usr/lib/rpm/striptofile -g -u -o $OUTPUTDIR $f || :
done

mkdir -p ${RPM_BUILD_ROOT}/usr/src/debug
(DIR=`pwd`; cd $RPM_BUILD_DIR; LANG=C sort $DIR/$SOURCEFILE -z -u | cpio -pd0m ${RPM_BUILD_ROOT}/usr/src/debug)
# stupid cpio creates new directories in mode 0700, fixup
find ${RPM_BUILD_ROOT}/usr/src/debug -type d -print0 | xargs -0 chmod a+rx

find ${RPM_BUILD_ROOT}/usr/lib/debug -type f | sed -n -e "s#^$RPM_BUILD_ROOT#/#p" > $LISTFILE
find ${RPM_BUILD_ROOT}/usr/src/debug -mindepth 1 -maxdepth 1 | sed -n -e "s#^$RPM_BUILD_ROOT#/#p" >> $LISTFILE
