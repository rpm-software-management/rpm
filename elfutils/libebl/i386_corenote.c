/* i386 specific core note handling.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>

#include <libebl_i386.h>


/* We cannot include <sys/procfs.h> since it is available only on x86
   systems.  */
struct elf_prstatus
  {
    struct
    {
      int si_signo;			/* Signal number.  */
      int si_code;			/* Extra code.  */
      int si_errno;			/* Errno.  */
    } pr_info;				/* Info associated with signal.  */
    short int pr_cursig;		/* Current signal.  */
    unsigned long int pr_sigpend;	/* Set of pending signals.  */
    unsigned long int pr_sighold;	/* Set of held signals.  */
    __pid_t pr_pid;
    __pid_t pr_ppid;
    __pid_t pr_pgrp;
    __pid_t pr_sid;
    struct timeval pr_utime;		/* User time.  */
    struct timeval pr_stime;		/* System time.  */
    struct timeval pr_cutime;		/* Cumulative user time.  */
    struct timeval pr_cstime;		/* Cumulative system time.  */
    unsigned long int pr_reg[17];	/* GP registers.  */
    int pr_fpvalid;			/* True if math copro being used.  */
  };


struct elf_prpsinfo
  {
    char pr_state;			/* Numeric process state.  */
    char pr_sname;			/* Char for pr_state.  */
    char pr_zomb;			/* Zombie.  */
    char pr_nice;			/* Nice val.  */
    unsigned long int pr_flag;		/* Flags.  */
    unsigned short int pr_uid;
    unsigned short int pr_gid;
    int pr_pid, pr_ppid, pr_pgrp, pr_sid;
    /* Lots missing */
    char pr_fname[16];			/* Filename of executable.  */
    char pr_psargs[80];			/* Initial part of arg list.  */
  };


bool
i386_core_note (name, type, descsz, desc)
     const char *name;
     uint32_t type;
     uint32_t descsz;
     const char *desc;
{
  bool result = false;

  switch (type)
    {
    case NT_PRSTATUS:
      if (descsz < sizeof (struct elf_prstatus))
	/* Not enough data.  */
	break;

      struct elf_prstatus *stat = (struct elf_prstatus *) desc;

      printf ("    SIGINFO:  signo: %d, code = %d, errno = %d\n"
	      "    signal: %hd, pending: %#08lx, holding: %#08lx\n"
	      "    pid: %d, ppid = %d, pgrp = %d, sid = %d\n"
	      "     utime: %6ld.%06lds,  stime: %6ld.%06lds\n"
	      "    cutime: %6ld.%06lds, cstime: %6ld.%06lds\n"
	      "    eax: %08lx  ebx: %08lx  ecx: %08lx  edx: %08lx\n"
	      "    esi: %08lx  edi: %08lx  ebp: %08lx  esp: %08lx\n"
	      "    eip: %08lx  eflags: %08lx, original eax: %08lx\n"
	      "    cs: %04lx  ds: %04lx  es: %04lx  fs: %04lx  gs: %04lx"
	      "  ss: %04lx\n\n"
,
	      stat->pr_info. si_signo,
	      stat->pr_info. si_code,
	      stat->pr_info. si_errno,
	      stat->pr_cursig,
	      stat->pr_sigpend, stat->pr_sighold,
	      stat->pr_pid, stat->pr_ppid, stat->pr_pgrp, stat->pr_sid,
	      stat->pr_utime.tv_sec, stat->pr_utime.tv_usec,
	      stat->pr_stime.tv_sec, stat->pr_stime.tv_usec,
	      stat->pr_cutime.tv_sec, stat->pr_cutime.tv_usec,
	      stat->pr_cstime.tv_sec, stat->pr_cstime.tv_usec,
	      stat->pr_reg[6], stat->pr_reg[0], stat->pr_reg[1],
	      stat->pr_reg[2], stat->pr_reg[3], stat->pr_reg[4],
	      stat->pr_reg[5], stat->pr_reg[15], stat->pr_reg[12],
	      stat->pr_reg[14], stat->pr_reg[11], stat->pr_reg[13] & 0xffff,
	      stat->pr_reg[7] & 0xffff, stat->pr_reg[8] & 0xffff,
	      stat->pr_reg[9] & 0xffff, stat->pr_reg[10] & 0xffff,
	      stat->pr_reg[16]);

      /* We handled this entry.  */
      result = true;
      break;

    case NT_PRPSINFO:
      if (descsz < sizeof (struct elf_prpsinfo))
	/* Not enough data.  */
	break;

      struct elf_prpsinfo *info = (struct elf_prpsinfo *) desc;

      printf ("    state: %c (%hhd),  zombie: %hhd,  nice: %hhd\n"
	      "    flags: %08lx,  uid: %hd,  gid: %hd\n"
	      "    pid: %d,  ppid: %d,  pgrp: %d,  sid: %d\n"
	      "    fname: %.16s\n"
	      "     args: %.80s\n\n",
	      info->pr_sname, info->pr_state, info->pr_zomb, info->pr_nice,
	      info->pr_flag, info->pr_uid, info->pr_gid,
	      info->pr_pid, info->pr_ppid, info->pr_pgrp, info->pr_sid,
	      info->pr_fname, info->pr_psargs);

      /* We handled this entry.  */
      result = true;
      break;

    default:
      break;
    }

  return result;
}
