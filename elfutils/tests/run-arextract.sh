#! /bin/sh
# Copyright (C) 1999, 2000, 2002 Red Hat, Inc.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

if (echo "testing\c"; echo 1,2,3) | grep c >/dev/null; then
  # Stardent Vistra SVR4 grep lacks -e, says ghazi@caip.rutgers.edu.
  if (echo -n testing; echo 1,2,3) | sed s/-n/xn/ | grep xn >/dev/null; then
    ac_n= ac_c='
' ac_t='	'
  else
    ac_n=-n ac_c= ac_t=
  fi
else
  ac_n= ac_c='\c' ac_t=
fi

archive=../libelf/.libs/libelf.a
test -f $archive || archive=../libelf/libelf.a
if test -f $archive; then
    # The file is really available (i.e., no shared-only built).
    echo $ac_n "Extracting symbols... $ac_c"

    # The files we are looking at.
    for f in ../libelf/*.o; do
	./arextract $archive `basename $f` arextract.test || exit 1
	cmp $f arextract.test || {
	    echo "Extraction of $1 failed"
	    exit 1
	}
	rm -f ${objpfx}arextract.test
    done

    echo "done"
fi

exit 0
