
/* Copyright (C) 2002, 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include "system.h"

#include <pthread.h>
#include <signal.h>

#include <rpmlib.h>
#include <rpmts.h>
#include "psm.h"

#include "debug.h"

#define _PSM_DEBUG      0
/*@unchecked@*/
int _psm_debug = _PSM_DEBUG;

#define	SHELL_PATH	"/bin/sh"	/* Path of the shell.  */
#define	SHELL_NAME	"sh"		/* Name to give it.  */


/**
 */
/*@unchecked@*/
static sigset_t caught;

/**
 */
/*@unchecked@*/
static struct psmtbl_s {
    int nalloced;
    int npsms;
/*@null@*/
    rpmpsm * psms;
} psmtbl = { 0, 0, NULL };

/* forward ref */
static void handler(int signum)
	/*@globals caught, psmtbl, fileSystem @*/
	/*@modifies caught, psmtbl, fileSystem @*/;

/**
 */
/*@unchecked@*/
static pthread_mutex_t satbl_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 */
/*@unchecked@*/
/*@-fullinitblock@*/
static struct sigtbl_s {
    int signum;
    int active;
    void (*handler) (int signum);
    struct sigaction oact;
} satbl[] = {
    { SIGINT,   0, handler },
#define	satbl_sigint	(&satbl[0])
    { SIGQUIT,  0, handler },
#define	satbl_sigquit	(&satbl[1])
    { SIGCHLD,	0, handler },
#define	satbl_sigchld	(&satbl[2])
#define	sigchld_active	satbl_sigchld->active
#define	DO_LOCK()	pthread_mutex_lock(&satbl_lock);
#define	DO_UNLOCK()	pthread_mutex_unlock(&satbl_lock);
#define	INIT_LOCK()	\
	{ pthread_mutex_init (&satbl_lock, NULL); sigchld_active = 0; }
#define	ADD_REF()	sigchld_active++
#define	SUB_REF()	--sigchld_active

#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr) \
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, (__oldtypeptr)); \
	pthread_cleanup_push((__handler), (__arg));
#define	CLEANUP_RESET(__execute, __oldtype) \
	pthread_cleanup_pop(__execute); \
	pthread_setcanceltype ((__oldtype), &(__oldtype));

    { SIGHUP,   0, handler },
#define	satbl_sighup	(&satbl[3])
    { SIGTERM,  0, handler },
#define	satbl_sigterm	(&satbl[4])
    { SIGPIPE,  0, handler },
#define	satbl_sigpipe	(&satbl[5])
    { -1,	0, NULL },
};
typedef struct sigtbl_s * sigtbl;
/*@=fullinitblock@*/

/**
 */
/*@-incondefs@*/
static void handler(int signum)
{
    sigtbl tbl;

    for (tbl = satbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum != signum)
	    continue;
	if (!tbl->active)
	    continue;
	(void) sigaddset(&caught, signum);
	switch (signum) {
	case SIGCHLD:
	    while (1) {
		int status = 0;
		pid_t reaped = waitpid(0, &status, WNOHANG);
		int i;

		if (reaped <= 0)
		    /*@innerbreak@*/ break;

		if (psmtbl.psms)
		for (i = 0; i < psmtbl.npsms; i++) {
		    rpmpsm psm = psmtbl.psms[i];
		    if (psm->child != reaped)
			/*@innercontinue@*/ continue;

#if _PSM_DEBUG
/*@-modfilesys@*/
if (_psm_debug)
fprintf(stderr, "      Reap: %p[%d:%d:%d] = %p child %d\n", psmtbl.psms, i, psmtbl.npsms, psmtbl.nalloced, psm, psm->child);
/*@=modfilesys@*/
#endif

		    psm->reaped = reaped;
		    psm->status = status;
		    /*@innerbreak@*/ break;
		}
	    }
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
	break;
    }
}
/*@=incondefs@*/

/* The cancellation handler.  */
static void
sigchld_cancel (void *arg)
{
    pid_t child = *(pid_t *) arg;
    pid_t result;
    int err;

    err = kill(child, SIGKILL);

    do {
	result = waitpid(child, NULL, 0);
    } while (result == (pid_t)-1 && errno == EINTR);

    DO_LOCK ();
    if (SUB_REF () == 0) {
	(void) sigaction (SIGQUIT, &satbl_sigquit->oact, (struct sigaction *) NULL);
	(void) sigaction (SIGINT, &satbl_sigint->oact, (struct sigaction *) NULL);
    }
    DO_UNLOCK ();
}

/* Execute LINE as a shell command, returning its status.  */
static int
do_system (const char *line)
{
    int oldtype;
    int status = -1;
    pid_t pid;
    pid_t result;
    struct sigaction sa;
    sigset_t omask;

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);

    DO_LOCK ();
    if (ADD_REF () == 0) {
	if (sigaction (SIGINT, &sa, &satbl_sigint->oact) < 0) {
	    SUB_REF ();
	    goto out;
	}
	if (sigaction (SIGQUIT, &sa, &satbl_sigquit->oact) < 0) {
	    SUB_REF ();
	    goto out_restore_sigint;
	}
    }
    DO_UNLOCK ();

    /* We reuse the bitmap in the 'sa' structure.  */
    sigaddset (&sa.sa_mask, SIGCHLD);
    if (sigprocmask (SIG_BLOCK, &sa.sa_mask, &omask) < 0) {
	DO_LOCK ();
	if (SUB_REF () == 0)
	    goto out_restore_sigquit_and_sigint;
	goto out;
    }

    CLEANUP_HANDLER(sigchld_cancel, &pid, &oldtype);

    pid = fork ();
    if (pid == (pid_t) 0) {
	/* Child side.  */
	const char *new_argv[4];
	new_argv[0] = SHELL_NAME;
	new_argv[1] = "-c";
	new_argv[2] = line;
	new_argv[3] = NULL;

	/* Restore the signals.  */
	(void) sigaction (SIGINT, &satbl_sigint->oact, NULL);
	(void) sigaction (SIGQUIT, &satbl_sigquit->oact, NULL);
	(void) sigprocmask (SIG_SETMASK, &omask, NULL);

	INIT_LOCK ();

	/* Exec the shell.  */
	(void) execve (SHELL_PATH, (char *const *) new_argv, __environ);
	_exit (127);
    } else if (pid < (pid_t) 0) {
	/* The fork failed.  */
	goto out;
    } else {
	/* Parent side.  */
	do {
	    result = waitpid(pid, &status, 0);
	} while (result == (pid_t)-1 && errno == EINTR);
	if (result != pid)
	    status = -1;
    }

    CLEANUP_RESET(0, oldtype);

    DO_LOCK ();
    if ((SUB_REF () == 0
       && (sigaction (SIGINT, &satbl_sigint->oact, NULL)
	   | sigaction (SIGQUIT, &satbl_sigquit->oact, NULL)) != 0)
       || sigprocmask (SIG_SETMASK, &omask, (sigset_t *) NULL) != 0)
    {
	status = -1;
    }
    goto out;

out_restore_sigquit_and_sigint:
    (void) sigaction (SIGQUIT, &satbl_sigquit->oact, NULL);
out_restore_sigint:
    (void) sigaction (SIGINT, &satbl_sigint->oact, NULL);
out:
    DO_UNLOCK ();
    return status;
}

static int
xsystem (const char * line)
{

    if (line == NULL)
	/* Check that we have a command processor available.  It might
	   not be available after a chroot(), for example.  */
	return do_system ("exit 0") == 0;

    return do_system (line);
}

static void *
other_thread(void *dat)
{
    const char * s = (const char *)dat;
    return ((void *) xsystem(s));
}

int main(int argc, char *argv[])
{
    pthread_t pth;

    pthread_create(&pth, NULL, other_thread, "/bin/sleep 30" );

    sleep(2);
    pthread_cancel(pth);

    pthread_join(pth, NULL);
    return 0;
}
