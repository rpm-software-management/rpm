#!/bin/sh

rm -f debug.log
rm -f child.log

# enable an safety-checking malloc in glibc which will abort() if
# heap corruption is detected.
MALLOC_CHECK_=2

export MALLOC_CHECK_

for f in $*; do
    if ${HARNESS} ./$f ${SRCDIR}; then
	:
    else
	echo FAILURE
	[ -z "$CARRYON" ] && exit 1
    fi
done

exit 0
