#! /bin/sh
# Copyright (C) 1999, 2000, 2002 Red Hat, Inc.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
#
# This program is Open Source software; you can redistribute it and/or
# modify it under the terms of the Open Software License version 1.0 as
# published by the Open Source Initiative.
#
# You should have received a copy of the Open Software License along
# with this program; if not, you may obtain a copy of the Open Software
# License version 1.0 from http://www.opensource.org/licenses/osl.php or
# by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
# 3001 King Ranch Road, Ukiah, CA 95482.
set -e

./get-pubnames2 $srcdir/testfile $srcdir/testfile2 > get-pubnames2.out

#diff -u get-pubnames2.out - <<"EOF"
# [ 0] "main", die: 104, cu: 11
#CU name: "m.c"
#object name: "main"
# [ 1] "a", die: 174, cu: 11
#CU name: "m.c"
#object name: "a"
# [ 2] "bar", die: 295, cu: 202
#CU name: "b.c"
#object name: "bar"
# [ 3] "foo", die: 5721, cu: 5628
#CU name: "f.c"
#object name: "foo"
# [ 0] "bar", die: 72, cu: 11
#CU name: "b.c"
#object name: "bar"
# [ 1] "foo", die: 2490, cu: 2429
#CU name: "f.c"
#object name: "foo"
# [ 2] "main", die: 2593, cu: 2532
#CU name: "m.c"
#object name: "main"
# [ 3] "a", die: 2663, cu: 2532
#CU name: "m.c"
#object name: "a"
#EOF
diff -u get-pubnames2.out - <<"EOF"
 [ 0] "main", die: 104, cu: 11
 [ 1] "a", die: 174, cu: 11
 [ 2] "bar", die: 295, cu: 202
 [ 3] "foo", die: 5721, cu: 5628
 [ 0] "bar", die: 72, cu: 11
 [ 1] "foo", die: 2490, cu: 2429
 [ 2] "main", die: 2593, cu: 2532
 [ 3] "a", die: 2663, cu: 2532
EOF

rm -f get-pubnames2.out

exit 0
