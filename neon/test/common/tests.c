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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "ne_utils.h"
#include "ne_socket.h"

#include "tests.h"

const char *name = NULL;
static FILE *child_debug;

static int passes = 0, fails = 0, aborted = 0;
static const char *suite;

void i_am(const char *testname)
{
    name = testname;
}

#define TEST_DEBUG (NE_DBG_HTTP | NE_DBG_SOCKET)

#define W(m) write(0, m, strlen(m))

static void child_segv(int signo)
{
    signal(SIGSEGV, SIG_DFL);
    W("(possible segfault in child, not dumping core)\n");
    exit(-1);
}

void in_child(void)
{
    ne_debug_init(child_debug, TEST_DEBUG);    
    NE_DEBUG(TEST_DEBUG, "**** Child forked ****\n");
    signal(SIGSEGV, child_segv);
}

int main(int argc, char *argv[])
{
    int n;
    FILE *debug;
    
    /* get basename(argv[0]) */
    suite = strrchr(argv[0], '/');
    if (suite == NULL) {
	suite = argv[0];
    } else {
	suite++;
    }
    
    sock_init();

    debug = fopen("debug.log", "a");
    if (debug == NULL) {
	fprintf(stderr, "%s: Could not open debug.log: %s\n", suite,
		strerror(errno));
	return -1;
    }
    child_debug = fopen("child.log", "a");
    if (child_debug == NULL) {
	fprintf(stderr, "%s: Could not open child.log: %s\n", suite,
		strerror(errno));
	fclose(debug);
	return -1;
    }

    ne_debug_init(debug, TEST_DEBUG);

    if (tests[0] == NULL) {
	printf("-> no tests found in %s\n", suite);
	return -1;
    }

    printf("-> running %s:\n", suite);
    
    for (n = 0; !aborted && tests[n] != NULL; n++) {
	printf("%d: ", n);
	name = NULL;
	fflush(stdout);
	NE_DEBUG(TEST_DEBUG, "******* Running test %d ********\n", n);
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
	    printf("FAIL.\n");
	    fails++;
	    break;
	}
    }

    printf("-> summary for %s: of %d tests: %d passed, %d failed. %.1f%%\n", 
	   suite, n, passes, fails, 100*(float)passes/n);

    if (fclose(debug)) {
	fprintf(stderr, "Error closing debug.log: %s\n", strerror(errno));
	fails = 1;
    }
       
    if (fclose(child_debug)) {
	fprintf(stderr, "Error closing child.log: %s\n", strerror(errno));
	fails = 1;
    }
    
    return fails;
}

