#!/bin/sh

for f in $*; do
    if ./$f; then
	:
    else
	echo FAILURE
	exit -1
    fi
done

exit 0
