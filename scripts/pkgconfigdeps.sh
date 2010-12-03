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

$pkgconfig --atleast-pkgconfig-version="0.24" || {
    cat > /dev/null
    exit 0
}

case $1 in
-P|--provides)
    while read filename ; do
    case "${filename}" in
    *.pc)
	# Query the dependencies of the package.
	DIR="`dirname ${filename}`"
	export PKG_CONFIG_PATH="$DIR:$DIR/../../share/pkgconfig"
	$pkgconfig --print-provides "$filename" 2> /dev/null | while read n r v ; do
	    [ -n "$n" ] || continue
	    # We have a dependency.  Make a note that we need the pkgconfig
	    # tool for this package.
	    echo -n "pkgconfig($n) "
	    [ -n "$r" ] && [ -n "$v" ] && echo -n "$r" "$v"
	    echo
	done
	;;
    esac
    done
    ;;
-R|--requires)
    while read filename ; do
    case "${filename}" in
    *.pc)
	i="`expr $i + 1`"
	[ $i -eq 1 ] && echo "$pkgconfig"
	DIR="`dirname ${filename}`"
	export PKG_CONFIG_PATH="$DIR:$DIR/../../share/pkgconfig"
	$pkgconfig --print-requires --print-requires-private "$filename" 2> /dev/null | while read n r v ; do
	    [ -n "$n" ] || continue
	    echo -n "pkgconfig($n) "
	    [ -n "$r" ] && [ -n "$v" ] && echo -n "$r" "$v"
	    echo
	done
    esac
    done
    ;;
esac
exit 0
