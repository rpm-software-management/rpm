/* 
   Stupidly simple test framework
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

#include <sys/signal.h>

#include <stdio.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "tests.h"

static const char *name = NULL;
static int passes = 0, fails = 0, aborted = 0;
static const char *suite;

void i_am(const char *testname)
{
    name = testname;
}

static void segv(int signo)
{
    write(0, "SEGFAULT.\n", 10);
    exit(-1);
}

int main(int argc, char *argv[]) {
    int n;
    
    /* get basename(argv[0]) */
    suite = strrchr(argv[0], '/');
    if (suite == NULL) {
	suite = argv[0];
    } else {
	suite++;
    }

    (void) signal(SIGSEGV, segv);

    if (tests[0] == NULL) {
	printf("-> no tests found in %s\n", suite);
	return -1;
    }

    printf("-> running %s:\n", suite);
    
    for (n = 0; !aborted && tests[n] != NULL; n++) {
	printf("%d: ", n);
	name = NULL;
	fflush(stdout);
	switch (tests[n]()) {
	case OK:
	    printf("pass.\n");
	    passes++;
	    break;
	case FAILHARD:
	    aborted = 1;
	    /* fall-through */
	case FAIL:
	    if (name != NULL) {
		printf("%s - ", name);
	    }
	    printf("fail.\n");
	    fails++;
	    break;
	}
    }

    printf("-> summary for %s: of %d tests: %d passed, %d failed. %.1f%%\n", 
	   suite, n, passes, fails, 100*(float)passes/n);
    
    return fails;
}

