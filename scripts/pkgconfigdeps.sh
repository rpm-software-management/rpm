#!/bin/bash

pkgconfig=/usr/bin/pkg-config
test -x $pkgconfig || {
    cat > /dev/null
    exit 0
}

[ $# -ge 1 ] || {
    cat > /dev/null
    exit 0
}

case $1 in
-P|--provides)
    while read filename ; do
    case "${filename}" in
    *.pc)
	# Query the dependencies of the package.
	$pkgconfig --print-provides "$filename" 2> /dev/null | while read n r v ; do
	    # We have a dependency.  Make a note that we need the pkgconfig
	    # tool for this package.
	    echo "pkgconfig($n)" "$r" "$v"
	done
	;;
    esac
    done
    ;;
-R|--requires)
    while read filename ; do
    case "${filename}" in
    *.pc)
	$pkgconfig --print-requires "$filename" 2> /dev/null | while read n r v ; do
	    i="`expr $i + 1`"
	    [ $i -eq 1 ] && echo "pkgconfig"
	    echo "pkgconfig($n)" "$r" "$v"
	done
    esac
    done
    ;;
esac
exit 0
