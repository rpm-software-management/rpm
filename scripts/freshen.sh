#!/bin/sh

# Implements --freshen option in RPM. --freshen is mostly like upgrade, but
# go through each file and make sure the package is actually installed before
# upgrading it. This won't work properly if there are any odd options
# specified (i.e. filenames with " specified). I'm sure some shell-hacker
# out there can fix that <hint, hint>.

if [ $1 = ";" ]; then
    RPM=rpm
    shift
else
    RPM=$1
    shift 2
fi

args="-U"
while [ $# -gt 0 ]; do
    if [ "$1" = "--" ]; then
	break
    fi
    args="$args $1"
    shift
done

if [ $# = 0 ]; then
    exec $RPM $args
fi

origargs=$args
args="$args -- "
shift

# Just filenames left now
for n in $*; do
    # if the file doesn't exist, we'll let RPM give the error message
    if [ ! -f $n ]; then
	args="$args $n"
    else
	if ! rpm -q `rpm -qp $n` >/dev/null 2>&1; then
	    name=`rpm --qf "%{NAME}" -qp $n`
	    $RPM -q $name >/dev/null 2>&1 && args="$args $n"
	fi
    fi
done

if [ "$args" = "$origargs -- " ]; then
    echo no packages require freshening
else
    exec $RPM $args
fi
