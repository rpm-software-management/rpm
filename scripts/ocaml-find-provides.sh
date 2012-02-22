#!/bin/sh -
# OCaml-specific "find-provides" for RPM.
# By Richard W.M. Jones <rjones@redhat.com>
# $Id: ocaml-find-provides.sh,v 1.2 2007/09/06 11:49:59 rjones Exp $

#set -x

# Usage:
#   (1) If you don't want the module to depend on the exact compiler
#   version then use ocaml-find-requires.sh -c, but this is not something
#   you should do normally.
#
#   (2) For any modules which you want to ignore, use '-i Modulename'.

OCAMLOBJINFO=ocamlobjinfo
TEMP=`getopt -o i:f: -n ocaml-find-provides.sh -- "$@"`
if [ $? != 0 ]; then echo "ocaml-find-provides.sh: failed" >&2; exit 1; fi
eval set -- "$TEMP"

ignore_modules=nOTREAL

while true; do
    case "$1" in
	-i) ignore_modules="$2 $ignore_modules"; shift 2;;
	-f) OCAMLOBJINFO="$2"; shift 2;;
	--) shift; break;;
	*) echo "ocaml-find-provides.sh: option error at $1"; exit 1;;
    esac
done

# Get the list of files.
files=`sed "s/['\"]/\\\&/g"`

# Get list of .cmi, .cmo and .cma files.
files=`echo $files | tr '[:blank:]' '\n' | grep '\.cm[ioa]$'`

if [ -z "$files" ]; then exit 0; fi

# Get the list of modules exported by the files.
modules=`$OCAMLOBJINFO $files |
          grep -E '(Unit|Module) name: ' |
          awk '{print $3}'`

# Turn list of modules into a regexp that matches the module names.
modules_re=`echo $modules | sed 's/ /|/g'`
ignore_modules_re=`echo $ignore_modules | sed 's/ /|/g'`

# Get a list of the modules these file(s) provide.
$OCAMLOBJINFO $files |
grep -Eo '[0-9a-f]{32}[[:space:]]+[A-Za-z0-9_]+' |
grep -E '[0-9a-f]{32}[[:space:]]+'"($modules_re)\$" |
while read md5sum module; do
    echo "ocaml($module) = $md5sum"
done |
grep -Ev "$ignore_modules_re" |
sort -u
