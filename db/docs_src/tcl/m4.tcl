m4_comment([$Id: m4.tcl,v 11.28 2004/12/16 19:13:05 bostic Exp $])

define(m4_tcl_version,		[m4_bold(berkdb version)])

define(m4_tcl_env_open,		[m4_bold(berkdb env)])
define(m4_tcl_env_close,	[m4_italic(env) m4_bold(close)])
define(m4_tcl_env_remove,	[m4_bold(berkdb envremove)])
define(m4_tcl_env_dbremove,	[m4_italic(env) m4_bold(dbremove)])
define(m4_tcl_env_dbrename,	[m4_italic(env) m4_bold(dbrename)])

define(m4_tcl_db_open,		[m4_bold(berkdb open)])
define(m4_tcl_db_remove,	[m4_bold(berkdb dbremove)])
define(m4_tcl_db_rename,	[m4_bold(berkdb dbrename)])
define(m4_tcl_db_close,		[m4_italic(db) m4_bold(close)])
define(m4_tcl_db_count,		[m4_italic(db) m4_bold(count)])
define(m4_tcl_db_del,		[m4_italic(db) m4_bold(del)])
define(m4_tcl_db_get,		[m4_italic(db) m4_bold(get)])
define(m4_tcl_db_get_join,	[m4_italic(db) m4_bold(get_join)])
define(m4_tcl_db_get_type,	[m4_italic(db) m4_bold(get_type)])
define(m4_tcl_db_is_byteswapped,[m4_italic(db) m4_bold(is_byteswapped)])
define(m4_tcl_db_join,		[m4_italic(db) m4_bold(join)])
define(m4_tcl_db_put,		[m4_italic(db) m4_bold(put)])
define(m4_tcl_db_stat,		[m4_italic(db) m4_bold(stat)])
define(m4_tcl_db_sync,		[m4_italic(db) m4_bold(sync)])
define(m4_tcl_db_truncate,	[m4_italic(db) m4_bold(truncate)])

define(m4_tcl_db_cursor,	[m4_italic(db) m4_bold(cursor)])
define(m4_tcl_dbc_close,	[m4_italic(dbc) m4_bold(close)])
define(m4_tcl_dbc_del,		[m4_italic(dbc) m4_bold(del)])
define(m4_tcl_dbc_dup,		[m4_italic(dbc) m4_bold(dup)])
define(m4_tcl_dbc_get,		[m4_italic(dbc) m4_bold(get)])
define(m4_tcl_dbc_put,		[m4_italic(dbc) m4_bold(put)])

define(m4_tcl_txn,		[m4_italic(env) m4_bold(txn)])
define(m4_tcl_txn_commit,	[m4_italic(txn) m4_bold(commit)])
define(m4_tcl_txn_abort,	[m4_italic(txn) m4_bold(abort)])
define(m4_tcl_txn_ckp,		[m4_italic(env) m4_bold(txn_checkpoint)])

dnl m4_tcl_arg
dnl	Tcl function argument.
define(m4_tcl_arg, [__LB__$1__RB__])

dnl m4_tcl_txnopt:
dnl
dnl $1: auto if -auto_commit is a flag
dnl $1: env if an environment operation
define(m4_tcl_txnopt, [dnl
m4_tag([-txn txnid], [dnl
If the operation is part of an application-specified transaction, the
m4_arg(txnid) parameter is a transaction handle returned from
m4_tcl_txn.  If no transaction handle is specified, but the
ifelse([$1], auto, [-auto_commit flag is specified],
[operation occurs in a transactional
ifelse([$1], env, database environment, database)]),
the operation will be implicitly transaction protected.])])

dnl m4_tcl_ret_error:
dnl
define(m4_tcl_ret_error, [m4_p([In the case of error, a Tcl error is thrown.])])

dnl m4_tcl_ret_previous:
dnl	arg 1: command.
dnl
define(m4_tcl_ret_previous, [m4_p([dnl
Otherwise, the $1 command returns 0 on success, and in the case of error,
a Tcl error is thrown.])])

dnl m4_tcl_ret_standard:
dnl	arg 1: command.
dnl
define(m4_tcl_ret_standard, [m4_p([dnl
The $1 command returns 0 on success, and in the case of error, a Tcl error
is thrown.])])

define(m4_set_cachesize, [dnl
Set the size of the database's shared memory buffer pool (that is, the
cache), to m4_arg(gbytes) gigabytes plus m4_arg(bytes).  The cache
should be the size of the normal working data set of the application,
with some small amount of additional memory for unusual situations.
(Note: The working set is not the same as the number of simultaneously
referenced pages, and should be quite a bit larger!)

m4_p([dnl
The default cache size is 256KB, and may not be specified as less than
20KB.  Any cache size less than 500MB is automatically increased by 25%
to account for buffer pool overhead; cache sizes larger than 500MB are
used as specified.])

m4_p([dnl
It is possible to specify caches to m4_db that are large enough so that
they cannot be allocated contiguously on some architectures; for example,
some releases of Solaris limit the amount of memory that may be
allocated contiguously by a process.  If m4_arg(ncache) is 0 or 1, the
cache will be allocated contiguously in memory.  If it is greater than
1, the cache will be broken up into m4_arg(ncache) equally sized
separate pieces of memory.])

m4_p([dnl
For information on tuning the m4_db cache size, see
m4_link(M4RELDIR/ref/am_conf/cachesize, [Selecting a cache size]).])])

dnl m4_tcl_errfile
dnl	#1: if db or environment.
define(m4_tcl_errfile, [m4_p([dnl
When an error occurs in the m4_db library, a m4_db error or an error
return value is returned by the function. In some cases, however, the
errno value may be insufficient to completely describe the cause of the
error especially during initial application debugging.])
m4_p([dnl
The m4_arg(-errfile) argument is used to enhance the mechanism for
reporting error messages to the application by specifying a file to be
used for displaying additional m4_db error messages. In some cases, when
an error occurs, m4_db will output an additional error message to the
specified file reference.])
m4_p([dnl
ifelse($1, db, [dnl
The error message will consist of a Tcl command name and a colon (":"),
an error string, and a trailing m4_htmlquote(newline) character.  If
the database was opened in an environment, the Tcl command name will be
the environment name (for example, env0), otherwise it will be the
database command name (for example, db0).],[dnl The error message will
consist of the environment command name (for example, env0) and a colon
(":"), an error string, and a trailing m4_htmlquote(newline)
character.])])
m4_p([dnl
This error-logging enhancement does not slow performance or significantly
increase application size, and may be run during normal operation as well
as during application debugging.])])

define(m4_tcl_environ, [dnl
m4_tag([-use_environ], [dnl
The m4_db process' environment may be permitted to specify information
to be used when naming files; see
m4_link(M4RELDIR/ref/env/naming, [m4_db File Naming]).
Because permitting users to specify which files are used can create
security problems, environment information will be used in file naming
for all users only if the m4_arg(-use_environ) flag is set.])

m4_tag([-use_environ_root], [dnl
The m4_db process' environment may be permitted to specify information
to be used when naming files; see
m4_link(M4RELDIR/ref/env/naming, [m4_db File Naming]).
As permitting users to specify which files are used can create security
problems, if the m4_arg(-use_environ_root) flag is set, environment
information will be used for file naming only for users with appropriate
permissions (for example, users with a user-ID of 0 on m4_posix1_name
systems).])])

dnl The mode argument language.
dnl     #1: the subsystem name.
define(m4_tcl_filemode, [m4_p([dnl
On UNIX systems, or in m4_posix1_name environments, all files created by $1
are created with mode m4_arg(mode) (as described in m4_manref(chmod, 2)) and
modified by the process' umask value at the time of creation (see
m4_manref(umask, 2)).  The group ownership of created files is based on
the system and directory defaults, and is not further specified by m4_db.
If m4_arg(mode) is 0, files are created readable and writable by both
owner and group.  On Windows systems, the mode argument is ignored.])])

dnl Partial put language.
dnl	#1: the command name
define(m4_tcl_partial_put, [m4_p([dnl
The m4_arg(dlen) bytes starting m4_arg(doff) bytes from the beginning
of the specified key's data record are replaced by the data specified
by the data and size structure elements.  If m4_arg(dlen) is smaller
than the length of the supplied data, the record will grow; if
m4_arg(dlen) is larger than the length of the supplied data, the record
will shrink.  If the specified bytes do not exist, the record will be
extended using nul bytes as necessary, and the $1 call will succeed.
ifelse($1, m4_tcl_db_put, [dnl])])
m4_p([dnl
It is an error to attempt a partial put using the $1 command in a database
that supports duplicate records. Partial puts in databases supporting
duplicate records must be done using a m4_tcl_dbc_put command.])
m4_p([dnl
It is an error to attempt a partial put with differing m4_arg(dlen) and
supplied data length values in Queue or Recno databases with fixed-length
records.])])
