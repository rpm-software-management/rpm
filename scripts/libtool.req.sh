#!/bin/sh
while read possible ; do
	case "$possible" in
	*.la)
		for dep in `grep ^dependency_libs= "$possible" 2> /dev/null | \
		sed -r	-e "s,^dependency_libs='(.*)',\1,g"` ; do
			case "$dep" in
			/*.la)
				echo "libtool($dep)"
				;;
			esac
		done
		;;
	esac
done
