#!/bin/bash
pkgconfig=${1:-/usr/bin/pkg-config}
test -x $pkgconfig || exit 0
while read filename ; do
case "${filename}" in
*.pc)
	# Assume that this file doesn't contain useful information.
	needs_pkgconfig=false
	# Query the dependencies of the package.
	$pkgconfig --print-provides "$filename" 2> /dev/null | while read n r v ; do
		# We have a dependency.  Make a note that we need the pkgconfig
		# tool for this package.
		echo "pkgconfig($n)" "$r" "$v"
		needs_pkgconfig=true
	done
	# The dependency on the pkgconfig package itself.
	if $needs_pkgconfig ; then
		echo pkgconfig
	fi
	;;
esac
done
