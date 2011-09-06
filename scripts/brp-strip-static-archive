#!/bin/sh

if [ -z "$RPM_BUILD_ROOT" -o "$RPM_BUILD_ROOT" = "/" ]; then
	exit 0
fi

STRIP=${1:-strip}

case `uname -a` in
Darwin*) exit 0 ;;
*) ;;
esac

# Strip static libraries.
for f in `find "$RPM_BUILD_ROOT" -type f -a -exec file {} \; | \
        grep -v "^${RPM_BUILD_ROOT}/\?usr/lib/debug"  | \
	grep 'current ar archive' | \
	sed -n -e 's/^\(.*\):[ 	]*current ar archive/\1/p'`; do
	$STRIP -g "$f"
done
