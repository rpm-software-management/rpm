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

# Under pkgconf, disables dependency resolver
export PKG_CONFIG_MAXIMUM_TRAVERSE_DEPTH=1

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
	    if [ "$DIR" != "${DIR/i686-w64-mingw//}" ] ; then
		echo -n "mingw32-pkgconfig($n) "
	    elif [ "$DIR" != "${DIR/x86_64-w64-mingw//}" ] ; then
		echo -n "mingw64-pkgconfig($n) "
	    else
		echo -n "pkgconfig($n) "
	    fi
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
	    if [ "$DIR" != "${DIR/i686-w64-mingw//}" ] ; then
		echo -n "mingw32-pkgconfig($n) "
	    elif [ "$DIR" != "${DIR/x86_64-w64-mingw//}" ] ; then
		echo -n "mingw64-pkgconfig($n) "
	    else
		echo -n "pkgconfig($n) "
	    fi
	    [ -n "$r" ] && [ -n "$v" ] && echo -n "$r" "$v"
	    echo
	done
    esac
    done
    ;;
esac
exit 0
