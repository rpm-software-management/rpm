#!/bin/sh
#
# Convert per-system configuration in /etc/rpmrc to macros in /etc/rpm/macros.
#
# prereq: awk fileutils textutils sh-utils mktemp
#

RPMRC=$1
[ -z "$RPMRC" ] && RPMRC=/etc/rpmrc
MACROS=$2
[ -z "$MACROS" ] && MACROS=/etc/rpm/macros
# for testing
#RPMRC=/tmp/rpmrc
#MACROS=/tmp/macros

[ -f $RPMRC ] || exit 0

[ -f $MACROS ] && {
  echo "$MACROS already exists" 1>&2
  exit 1
}

DIRN="`dirname $MACROS`"
[ -d "$DIRN" ] || mkdir -p "$DIRN"
[ -d "$DIRN" ] || {
  echo "could not create directory $DIRN" 1>&2
  exit 1
}

TMP=$(mktemp /tmp/rpmrc.XXXXXX) || {
  echo could not create temp file 1>&2
  exit 1
}

awk 'BEGIN {
  macros="'"$MACROS"'"
  # direct translation except underscore prepended
  xlate["builddir"] = "_builddir"
  xlate["buildshell"] = "_buildshell"
  xlate["bzip2bin"] = "_bzip2bin"
  xlate["dbpath"] = "_dbpath"
  xlate["defaultdocdir"] = "_defaultdocdir"
  xlate["excludedocs"] = "_excludedocs"
  xlate["ftpport"] = "_ftpport"
  xlate["ftpproxy"] = "_ftpproxy"
  xlate["gzipbin"] = "_gzipbin"
  xlate["instchangelog"] = "_instchangelog"
  xlate["langpatt"] = "_langpatt"
  xlate["netsharedpath"] = "_netsharedpath"
  xlate["pgp_name"] = "_pgp_name"
  xlate["pgp_path"] = "_pgp_path"
  xlate["rpmdir"] = "_rpmdir"
  xlate["rpmfilename"] = "_rpmfilename"
  xlate["signature"] = "_signature"
  xlate["sourcedir"] = "_sourcedir"
  xlate["specdir"] = "_specdir"
  xlate["srcrpmdir"] = "_srcrpmdir"
  xlate["timecheck"] = "_timecheck"
  xlate["tmppath"] = "_tmppath"
  xlate["topdir"] = "_topdir"

  # direct translation with no underscore at all
  xlate["buildroot"] = "buildroot"
  xlate["distribution"] = "distribution"
  xlate["packager"] = "packager"
  xlate["vendor"] = "vendor"

  # simply remove
  xlate["messagelevel"] = ""
  xlate["require_distribution"] = ""
  xlate["require_icon"] = ""
  xlate["require_vendor"] = ""
}

{
  for (str in xlate) {
    ms = "^" str ":"
    if (match($1, ms)) {
      if (xlate[str]) {
        sub(ms, "%" xlate[str] " ")
        print >> macros
      }
      # else get ignore and thus get rid of obsolete items
      next
    }
    if (match ($1, "^fixperms:")) {
      sub("^fixperms:", "%_fixperms chmod -R ")
      print >> macros
      next
    }
  }
  print
  next
}
' < $RPMRC > $TMP || {
  echo "could not convert $RPMRC entries to $MACROS entries" 1>&2
  exit 1
}
if [ -s $TMP ] ; then
  # don't mess with timestamp unless we have actually changed something
  cat $TMP > $RPMRC && rm -f $TMP
  [ -f $TMP ] && { echo "could not overwrite $RPMRC" 1>&2 ; exit 1 ; }
fi
rm -f $TMP

exit 0
