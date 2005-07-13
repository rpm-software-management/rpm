#!/bin/bash
pkgconfig=${1:-/usr/bin/pkg-config}
test -x $pkgconfig || exit 0
while read filename ; do
case "${filename}" in
*.pc)
	$pkgconfig --print-requires "$filename" 2> /dev/null | while read n r v ; do
		echo "pkgconfig($n)" "$r" "$v"
	done
esac
done
