/* 
   String handling tests
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "string_utils.h"

#include "tests.h"

static int simple(void) {
    sbuffer s = sbuffer_create();
    ON(s == NULL);
    ON(sbuffer_zappend(s, "abcde"));
    ON(strcmp(sbuffer_data(s), "abcde"));
    ON(sbuffer_size(s) != 5);
    sbuffer_destroy(s);
    return OK;
}

static int concat(void) {
    sbuffer s = sbuffer_create();
    ON(s == NULL);
    ON(sbuffer_concat(s, "a", "b", "c", "d", "e", "f", "g", NULL));
    ON(strcmp(sbuffer_data(s), "abcdefg"));
    ON(sbuffer_size(s) != 7);
    sbuffer_destroy(s);
    return OK;
}

static int append(void) {
    sbuffer s = sbuffer_create();
    ON(s == NULL);
    ON(sbuffer_append(s, "a", 1));
    ON(sbuffer_append(s, "b", 1));
    ON(sbuffer_append(s, "c", 1));
    ON(strcmp(sbuffer_data(s), "abc"));
    ON(sbuffer_size(s) != 3);
    sbuffer_destroy(s);
    return OK;
}    

static int alter(void) {
    sbuffer s = sbuffer_create();
    char *d;
    ON(s == NULL);
    ON(sbuffer_zappend(s, "abcdefg"));
    d = sbuffer_data(s);
    ON(d == NULL);
    d[2] = '\0';
    sbuffer_altered(s);
    ON(strcmp(sbuffer_data(s), "ab"));
    ON(sbuffer_size(s) != 2);
    ON(sbuffer_zappend(s, "hijkl"));
    ON(strcmp(sbuffer_data(s), "abhijkl"));
    return OK;
}

test_func tests[] = {
    simple,
    concat,
    append,
    alter,
    NULL
};

