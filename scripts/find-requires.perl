#!/bin/sh

# note this works for both a.out and ELF executables
# it also auto-generates requirment lines for shell scripts

ulimit -c 0

filelist=`sed "s/['\"]/\\\&/g"`
scriptlist=`echo $filelist | xargs -r file | egrep ":.* (commands|script) " | cut -d: -f1 `

perllist=
for f in $scriptlist; do
    [ -x $f ] || continue
    interp=`head -1 $f | sed -e 's/^\#\![ 	]*//' | cut -d" " -f1 `
    case $interp in
    */perl) perllist="$perllist $f" ;;
    esac
done | sort -u

for f in $filelist; do
    [ -r $f ] || continue
    if echo $f | grep -q '\.pm$'
    then
        modules="$modules $f"
    fi
done

[ -n "$modules" ] && perllist="$perllist $modules"

#
# Generate perl module dependencies, if any.
set -x
[ -x /usr/lib/rpm/perl.req -a -n "$perllist" ] && \
	echo $perllist | tr [:blank:] \\n | /usr/lib/rpm/perl.req | sort -u

#
# Then process the files as usual.
set +x
echo $filelist | /usr/lib/rpm/find-requires
