#!/bin/sh

# note this works for both a.out and ELF executables
# it also auto-generates requirment lines for shell scripts

ulimit -c 0

filelist=`sed "s/['\"]/\\\&/g"`
exelist=`echo $filelist | xargs -r file | fgrep executable | cut -d: -f1 `
scriptlist=`echo $filelist | xargs -r file | egrep ":.* (commands|script) " | cut -d: -f1 `
liblist=`echo $filelist | xargs -r file | grep "shared object" | cut -d : -f1 `

for f in $exelist; do
    if [ -x $f ]; then
	ldd $f | awk '/=>/ { print $1 }'
    fi
done | sort -u | sed "s/['\"]/\\\&/g" | xargs -r -n 1 basename | grep -v 'libNoVersion.so' | grep -v '4[um]lib.so' | sort -u

for f in $liblist; do
    ldd $f | awk '/=>/ { print $1 }'
done | sort -u | sed "s/['\"]/\\\&/g" | xargs -r -n 1 basename | grep -v 'libNoVersion.so' | grep -v '4[um]lib.so' | sort -u

perllist=
for f in $scriptlist; do
    [ -x $f ] || continue
    interp=`head -1 $f | sed -e 's/^\#\![ 	]*//' | cut -d" " -f1 `
    case $interp in
    */perl) perllist="$perllist $f" ;;
    esac
    echo $interp
done | sort -u

for f in $liblist $exelist ; do
    objdump -p $f | awk '
	BEGIN { START=0; LIBNAME=""; }
	/Version References:/ { START=1; }
	/required from/ && (START==1) {
	    sub(/:/, "", $3);
	    LIBNAME=$3;
	}
	(START==1) && (LIBNAME!="") && ($4!="") { print LIBNAME "(" $4 ")"; }
	/^$/ { START=0; }
    '
done | sort -u

#
# Generate perl module dependencies, if any.
set -x
[ -x /usr/lib/rpm/perl.req -a -n "$perllist" ] && \
	echo $perllist | tr [:blank:] \\n | /usr/lib/rpm/perl.req | sort -u
