/* 
   Stupidly simple test framework
   Copyright (C) 2001-2002, Joe Orton <joe@manyfish.co.uk>

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

#include <sys/types.h>

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
#include "child.h"

char test_context[BUFSIZ];
int have_context = 0;

static FILE *child_debug, *debug;

char **test_argv;
int test_argc;

const char *test_suite;
int test_num;

/* statistics for all tests so far */
static int passes = 0, fails = 0, skipped = 0, warnings = 0;

/* per-test globals: */
static int warned, aborted = 0;
static const char *test_name; /* current test name */

static int use_colour = 0;

/* resource for ANSI escape codes:
 * http://www.isthe.com/chongo/tech/comp/ansi_escapes.html */
#define COL(x) do { if (use_colour) printf("\033[" x "m"); } while (0)

#define NOCOL COL("00")

void t_context(const char *context, ...)
{
    va_list ap;
    va_start(ap, context);
    ne_vsnprintf(test_context, BUFSIZ, context, ap);
    va_end(ap);
    have_context = 1;
}

void t_warning(const char *str, ...)
{
    va_list ap;
    COL("43;01"); printf("WARNING:"); NOCOL;
    putchar(' ');
    va_start(ap, str);
    vprintf(str, ap);
    va_end(ap);
    warnings++;
    warned++;
    putchar('\n');
}    

#define TEST_DEBUG \
(NE_DBG_HTTP | NE_DBG_SOCKET | NE_DBG_HTTPBODY | NE_DBG_HTTPAUTH | \
 NE_DBG_LOCKS | NE_DBG_XMLPARSE | NE_DBG_XML | NE_DBG_SSL)

#define W(m) write(0, m, strlen(m))

static void child_segv(int signo)
{
    signal(SIGSEGV, SIG_DFL);
    W("(possible segfault in child, not dumping core)\n");
    exit(-1);
}

static void segv(int signo)
{
    signal(SIGSEGV, SIG_DFL);
    if (use_colour) {
	W("\033[41;37;01m");
    }
    W("FAILED - segmentation fault.");
    if (use_colour) {
	W("\033[00m");
    }
    W("\n");
    fflush(debug);
    reap_server();
    kill(getpid(), SIGSEGV);
}

void in_child(void)
{
    ne_debug_init(child_debug, TEST_DEBUG);    
    NE_DEBUG(TEST_DEBUG, "**** Child forked for test %s ****\n", test_name);
    signal(SIGSEGV, child_segv);
}

int main(int argc, char *argv[])
{
    int n;
    static const char dots[] = "......................";
    
    /* get basename(argv[0]) */
    test_suite = strrchr(argv[0], '/');
    if (test_suite == NULL) {
	test_suite = argv[0];
    } else {
	test_suite++;
    }

#if defined(HAVE_ISATTY) && defined(STDOUT_FILENO)
    if (isatty(STDOUT_FILENO)) {
	use_colour = 1;
    }
#endif

    test_argc = argc;
    test_argv = argv;

    debug = fopen("debug.log", "a");
    if (debug == NULL) {
	fprintf(stderr, "%s: Could not open debug.log: %s\n", test_suite,
		strerror(errno));
	return -1;
    }
    child_debug = fopen("child.log", "a");
    if (child_debug == NULL) {
	fprintf(stderr, "%s: Could not open child.log: %s\n", test_suite,
		strerror(errno));
	fclose(debug);
	return -1;
    }

    if (tests[0].fn == NULL) {
	printf("-> no tests found in `%s'\n", test_suite);
	return -1;
    }

    /* install special SEGV handler. */
    signal(SIGSEGV, segv);

    /* test the "no-debugging" mode of ne_debug. */
    ne_debug_init(NULL, 0);
    NE_DEBUG(TEST_DEBUG, "This message should go to /dev/null");

    /* enable debugging for real. */
    ne_debug_init(debug, TEST_DEBUG);
    NE_DEBUG(TEST_DEBUG | NE_DBG_FLUSH, "Version string: %s\n", 
             ne_version_string());
    
    /* another silly test. */
    NE_DEBUG(0, "This message should also go to /dev/null");

    if (ne_sock_init()) {
	COL("43;01"); printf("WARNING:"); NOCOL;
	printf(" Socket library initalization failed.\n");
    }

    printf("-> running `%s':\n", test_suite);
    
    for (n = 0; !aborted && tests[n].fn != NULL; n++) {
	int result;
#ifdef NEON_MEMLEAK
        size_t allocated = ne_alloc_used;
#endif

	test_name = tests[n].name;
	printf("%2d. %s%.*s ", n, test_name, 
	       (int) (strlen(dots) - strlen(test_name)), dots);
	have_context = 0;
	test_num = n;
	warned = 0;
	fflush(stdout);
	NE_DEBUG(TEST_DEBUG, "******* Running test %d: %s ********\n", 
		 n, test_name);

	/* run the test. */
	result = tests[n].fn();

#ifdef NEON_MEMLEAK
        /* issue warnings for memory leaks, if requested */
        if ((tests[n].flags & T_CHECK_LEAKS) && result == OK &&
            ne_alloc_used > allocated) {
            t_context("memory leak of %" NE_FMT_SIZE_T " bytes",
                      ne_alloc_used - allocated);
            fprintf(debug, "Blocks leaked: ");
            ne_alloc_dump(debug);
            result = FAIL;
        } else if (tests[n].flags & T_EXPECT_LEAKS && result == OK &&
                   ne_alloc_used == allocated) {
            t_context("expected memory leak not detected");
            result = FAIL;
        }
#endif

        if (tests[n].flags & T_EXPECT_FAIL) {
            if (result == OK) {
                t_context("test passed but expected failure");
                result = FAIL;
            } else if (result == FAIL)
                result = OK;
        }

	/* align the result column if we've had warnings. */
	if (warned) {
	    printf("    %s ", dots);
	}

	switch (result) {
	case OK:
	    COL("32"); printf("pass"); NOCOL;
	    if (warned) {
		printf(" (with %d warning%s)", warned, (warned > 1)?"s":"");
	    }
	    putchar('\n');
	    passes++;
	    break;
	case FAILHARD:
	    aborted = 1;
	    /* fall-through */
	case FAIL:
	    COL("41;37;01"); printf("FAIL"); NOCOL;
	    if (have_context) {
		printf(" (%s)", test_context);
	    }
	    putchar('\n');
	    fails++;
	    break;
	case SKIPREST:
	    aborted = 1;
	    /* fall-through */
	case SKIP:
	    COL("44"); printf("SKIPPED"); NOCOL;
	    if (have_context) {
		printf(" (%s)", test_context);
	    }
	    putchar('\n');
	    skipped++;
	    break;
	default:
	    COL("41;37;01"); printf("OOPS"); NOCOL;
	    printf(" unexpected test result `%d'\n", result);
	    break;
	}

	reap_server();
    }

    /* discount skipped tests */
    if (skipped) {
	printf("-> %d %s.\n", skipped,
	       skipped==1?"test was skipped":"tests were skipped");
	n -= skipped;
	if (passes + fails != n) {
	    printf("-> ARGH! Number of test results does not match "
		   "number of tests.\n"
		   "-> ARGH! Test Results are INRELIABLE.\n");
	}
    }
    /* print the summary. */
    if (skipped && n == 0) {
	printf("<- all tests skipped for `%s'.\n", test_suite);
    } else {
	printf("<- summary for `%s': "
	       "of %d tests run: %d passed, %d failed. %.1f%%\n",
	       test_suite, n, passes, fails, 100*(float)passes/n);
	if (warnings) {
	    printf("-> %d warning%s issued.\n", warnings, 
		   warnings==1?" was":"s were");
	}
    }

    if (fclose(debug)) {
	fprintf(stderr, "Error closing debug.log: %s\n", strerror(errno));
	fails = 1;
    }
       
    if (fclose(child_debug)) {
	fprintf(stderr, "Error closing child.log: %s\n", strerror(errno));
	fails = 1;
    }

    ne_sock_exit();
    
    return fails;
}

