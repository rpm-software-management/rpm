#!/bin/sh
# Nicked from gnome-vfs/po, modified to be package-independant
#  -joe

xgettext --default-domain=$1 --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $1.po \
   || ( rm -f ./$1.pot \
    && mv $1.po ./$1.pot )
