
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

#include <sched.h>
#include <pthread.h>

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define	SHELL_PATH	"/bin/sh"	/* Path of the shell.  */
#define	SHELL_NAME	"sh"		/* Name to give it.  */

static struct sigaction intr;
static struct sigaction quit;

static int syssa_refcnt;

static pthread_mutex_t syssa_lock = PTHREAD_MUTEX_INITIALIZER;

/* =============================================================== */

/* We need some help from the assembler to generate optimal code.  We
   define some macros here which later will be used.  */
asm (".L__X'%ebx = 1\n\t"
     ".L__X'%ecx = 2\n\t"
     ".L__X'%edx = 2\n\t"
     ".L__X'%eax = 3\n\t"
     ".L__X'%esi = 3\n\t"
     ".L__X'%edi = 3\n\t"
     ".L__X'%ebp = 3\n\t"
     ".L__X'%esp = 3\n\t"
     ".macro bpushl name reg\n\t"
     ".if 1 - \\name\n\t"
     ".if 2 - \\name\n\t"
     "pushl %ebx\n\t"
     ".else\n\t"
     "xchgl \\reg, %ebx\n\t"
     ".endif\n\t"
     ".endif\n\t"
     ".endm\n\t"
     ".macro bpopl name reg\n\t"
     ".if 1 - \\name\n\t"
     ".if 2 - \\name\n\t"
     "popl %ebx\n\t"
     ".else\n\t"
     "xchgl \\reg, %ebx\n\t"
     ".endif\n\t"
     ".endif\n\t"
     ".endm\n\t"
     ".macro bmovl name reg\n\t"
     ".if 1 - \\name\n\t"
     ".if 2 - \\name\n\t"
     "movl \\reg, %ebx\n\t"
     ".endif\n\t"
     ".endif\n\t"
     ".endm\n\t");

/* Define a macro which expands inline into the wrapper code for a system
   call.  */
#undef INLINE_SYSCALL
#define INLINE_SYSCALL(name, nr, args...) \
  ({									      \
    unsigned int resultvar = INTERNAL_SYSCALL (name, , nr, args);	      \
    if (__builtin_expect (INTERNAL_SYSCALL_ERROR_P (resultvar, ), 0))	      \
      {									      \
	errno = (INTERNAL_SYSCALL_ERRNO (resultvar, ));			      \
	resultvar = 0xffffffff;						      \
      }									      \
    (int) resultvar; })

/* Define a macro which expands inline into the wrapper code for a system
   call.  This use is for internal calls that do not need to handle errors
   normally.  It will never touch errno.  This returns just what the kernel
   gave back.  */
# define INTERNAL_SYSCALL(name, err, nr, args...) \
  ({									      \
    unsigned int resultvar;						      \
    asm volatile (							      \
    LOADARGS_##nr							      \
    "movl %1, %%eax\n\t"						      \
    "int $0x80\n\t"						      \
    RESTOREARGS_##nr							      \
    : "=a" (resultvar)							      \
    : "i" (__NR_##name) ASMFMT_##nr(args) : "memory", "cc");		      \
    (int) resultvar; })

#undef INTERNAL_SYSCALL_DECL
#define INTERNAL_SYSCALL_DECL(err) do { } while (0)

#undef INTERNAL_SYSCALL_ERROR_P
#define INTERNAL_SYSCALL_ERROR_P(val, err) \
  ((unsigned int) (val) >= 0xfffff001u)

#undef INTERNAL_SYSCALL_ERRNO
#define INTERNAL_SYSCALL_ERRNO(val, err)	(-(val))

#define LOADARGS_0
# define LOADARGS_1 \
    "bpushl .L__X'%k3, %k3\n\t"						      \
    "bmovl .L__X'%k3, %k3\n\t"
#define LOADARGS_2	LOADARGS_1
#define LOADARGS_3	LOADARGS_1
#define LOADARGS_4	LOADARGS_1
#define LOADARGS_5	LOADARGS_1

#define RESTOREARGS_0
# define RESTOREARGS_1 \
    "bpopl .L__X'%k3, %k3\n\t"
#define RESTOREARGS_2	RESTOREARGS_1
#define RESTOREARGS_3	RESTOREARGS_1
#define RESTOREARGS_4	RESTOREARGS_1
#define RESTOREARGS_5	RESTOREARGS_1

#define ASMFMT_0()
#define ASMFMT_1(arg1) \
	, "acdSD" (arg1)
#define ASMFMT_2(arg1, arg2) \
	, "adSD" (arg1), "c" (arg2)
#define ASMFMT_3(arg1, arg2, arg3) \
	, "aSD" (arg1), "c" (arg2), "d" (arg3)
#define ASMFMT_4(arg1, arg2, arg3, arg4) \
	, "aD" (arg1), "c" (arg2), "d" (arg3), "S" (arg4)
#define ASMFMT_5(arg1, arg2, arg3, arg4, arg5) \
	, "a" (arg1), "c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5)

/* We have to and actually can handle cancelable system().  The big
   problem: we have to kill the child process if necessary.  To do
   this a cleanup handler has to be registered and is has to be able
   to find the PID of the child.  The main problem is to reliable have
   the PID when needed.  It is not necessary for the parent thread to
   return.  It might still be in the kernel when the cancellation
   request comes.  Therefore we have to use the clone() calls ability
   to have the kernel write the PID into the user-level variable.  */
# define FORK() \
  INLINE_SYSCALL (clone, 3, CLONE_PARENT_SETTID | SIGCHLD, 0, &pid)

/* =============================================================== */

/* The cancellation handler.  */
static void
cancel_handler (void *arg)
{
  pid_t child = *(pid_t *) arg;
  pid_t result;
  int err;

  err = kill(child, SIGKILL);

  do {
    result = waitpid(child, NULL, 0);
  } while (result == (pid_t)-1 && errno == EINTR);

  pthread_mutex_lock(&syssa_lock);
  if (--syssa_refcnt == 0) {
      (void) sigaction (SIGQUIT, &quit, (struct sigaction *) NULL);
      (void) sigaction (SIGINT, &intr, (struct sigaction *) NULL);
  }
  pthread_mutex_unlock(&syssa_lock);
}

/* Execute LINE as a shell command, returning its status.  */
static int
do_system (const char *line)
{
  int oldtype;
  int status;
  int save;
  pid_t pid;
  pid_t result;
  struct sigaction sa;
  sigset_t omask;

fprintf(stderr, "*** %s(%p) %s\n", __FUNCTION__, line, line);
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigemptyset (&sa.sa_mask);

  pthread_mutex_lock(&syssa_lock);
  if (syssa_refcnt++ == 0) {
      if (sigaction (SIGINT, &sa, &intr) < 0) {
	  --syssa_refcnt;
	  goto out;
      }
      if (sigaction (SIGQUIT, &sa, &quit) < 0) {
	  save = errno;
	  --syssa_refcnt;
	  goto out_restore_sigint;
      }
  }
  pthread_mutex_unlock(&syssa_lock);

  /* We reuse the bitmap in the 'sa' structure.  */
  sigaddset (&sa.sa_mask, SIGCHLD);
  save = errno;
  if (sigprocmask (SIG_BLOCK, &sa.sa_mask, &omask) < 0) {
          pthread_mutex_lock(&syssa_lock);
	  if (--syssa_refcnt == 0)
	    {
	      save = errno;
	      (void) sigaction (SIGQUIT, &quit, (struct sigaction *) NULL);
	    out_restore_sigint:
	      (void) sigaction (SIGINT, &intr, (struct sigaction *) NULL);
	      errno = save;
	    }
	out:
          pthread_mutex_unlock(&syssa_lock);
	  return -1;
    }

  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
  pthread_cleanup_push(cancel_handler, &pid);

/* int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg,
             pid_t *tid, struct user_desc *tls); */
/* INLINE_SYSCALL (clone, 3, CLONE_PARENT_SETTID | SIGCHLD, 0, &pid) */

/* pid = clone(sub, NULL, (CLONE_PARENT_SETTID | SIGCHLD), line,
		&pid???, &tls???); */

  pid = FORK ();
  if (pid == (pid_t) 0) {
    /* Child side.  */
    const char *new_argv[4];
    new_argv[0] = SHELL_NAME;
    new_argv[1] = "-c";
    new_argv[2] = line;
    new_argv[3] = NULL;

    /* Restore the signals.  */
    (void) sigaction (SIGINT, &intr, (struct sigaction *) NULL);
    (void) sigaction (SIGQUIT, &quit, (struct sigaction *) NULL);
    (void) sigprocmask (SIG_SETMASK, &omask, (sigset_t *) NULL);

    { pthread_mutex_init (&syssa_lock, NULL); syssa_refcnt = 0; }

    /* Exec the shell.  */
    (void) execve (SHELL_PATH, (char *const *) new_argv, __environ);
    _exit (127);
  } else if (pid < (pid_t) 0) {
    /* The fork failed.  */
    status = -1;
  } else {
    /* Parent side.  */
    do {
      result = waitpid(pid, &status, 0);
    } while (result == (pid_t)-1 && errno == EINTR);
    if (result != pid)
      status = -1;
  }

  pthread_cleanup_pop(0);
  pthread_setcanceltype (oldtype, &oldtype);

  save = errno;
  pthread_mutex_lock(&syssa_lock);
  if ((--syssa_refcnt == 0
       && (sigaction (SIGINT, &intr, (struct sigaction *) NULL)
	   | sigaction (SIGQUIT, &quit, (struct sigaction *) NULL)) != 0)
      || sigprocmask (SIG_SETMASK, &omask, (sigset_t *) NULL) != 0)
  {
	status = -1;
  }
  pthread_mutex_unlock(&syssa_lock);

  return status;
}

static int
system (const char * line)
{

fprintf(stderr, "*** %s(%p) %s\n", __FUNCTION__, line, line);
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
fprintf(stderr, "*** %s(%p)\n", __FUNCTION__, dat);
    return ((void *) system(s));
}

int main(int argc, char *argv[])
{
  pthread_t pth;

  pthread_create(&pth, NULL, other_thread, "/bin/pwd" );

  pthread_join(pth, NULL);

  return 0;
}

