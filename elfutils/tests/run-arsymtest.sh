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

lib=../libelf/libelf.a
okfile=arsymtest.ok
tmpfile=arsymtest.tmp
testfile=arsymtest.test

result=0
if test -f $lib; then
    # Generate list using `nm' we check against.
    nm -s lib |
    sed -e '1,/^Arch/d' -e '/^$/,$d' |
    sort > $okfile

    # Now run our program using libelf.
    ./arsymtest $lib $tmpfile || exit 1
    sort $tmpfile > $testfile
    rm $tmpfile

    # Compare the outputs.
    if cmp $okfile $testfile; then
	result=0
	rm $testfile $okfile
    else
	result=1
    fi
fi

exit $result
