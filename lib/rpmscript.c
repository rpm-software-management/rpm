#include "system.h"

#include <errno.h>
#include <unistd.h>

#define _RPMSQ_INTERNAL
#include <rpm/rpmsq.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmio.h>
#include <rpm/rpmlog.h>
#include <rpm/header.h>

#include "rpmio/rpmlua.h"
#include "lib/rpmscript.h"

#include "debug.h"

/**
 * Run internal Lua script.
 */
static rpmRC runLuaScript(int selinux, ARGV_const_t prefixes,
		   const char *sname, rpmlogLvl lvl, FD_t scriptFd,
		   ARGV_t * argvp, const char *script, int arg1, int arg2)
{
    rpmRC rc = RPMRC_FAIL;
#ifdef WITH_LUA
    ARGV_t argv = argvp ? *argvp : NULL;
    rpmlua lua = NULL; /* Global state. */
    rpmluav var;
    int cwd = -1;

    rpmlog(RPMLOG_DEBUG, "%s: running <lua> scriptlet.\n", sname);

    /* Create arg variable */
    rpmluaPushTable(lua, "arg");
    var = rpmluavNew();
    rpmluavSetListMode(var, 1);
    if (argv) {
	char **p;
	for (p = argv; *p; p++) {
	    rpmluavSetValue(var, RPMLUAV_STRING, *p);
	    rpmluaSetVar(lua, var);
	}
    }
    if (arg1 >= 0) {
	rpmluavSetValueNum(var, arg1);
	rpmluaSetVar(lua, var);
    }
    if (arg2 >= 0) {
	rpmluavSetValueNum(var, arg2);
	rpmluaSetVar(lua, var);
    }
    var = rpmluavFree(var);
    rpmluaPop(lua);

    /* Lua scripts can change our cwd and umask, save and restore */
    /* XXX TODO: use cwd from chroot state to save unnecessary open here */
    cwd = open(".", O_RDONLY);
    if (cwd != -1) {
	mode_t oldmask = umask(0);
	umask(oldmask);

	if (chdir("/") == 0 && rpmluaRunScript(lua, script, sname) == 0) {
	    rc = RPMRC_OK;
	}
	/* This failing would be fatal, return something different for it... */
	if (fchdir(cwd)) {
	    rpmlog(RPMLOG_ERR, _("Unable to restore current directory: %m"));
	    rc = RPMRC_NOTFOUND;
	}
	close(cwd);
	umask(oldmask);
    }

    rpmluaDelVar(lua, "arg");

#else
    rpmlog(lvl, _("<lua> scriptlet support not built in\n"));
#endif

    return rc;
}

static const char * const SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

static void doScriptExec(int selinux, ARGV_const_t argv, ARGV_const_t prefixes,
			FD_t scriptFd, FD_t out)
{
    int pipes[2];
    int flag;
    int fdno;
    int xx;
    int open_max;

    (void) signal(SIGPIPE, SIG_DFL);
    pipes[0] = pipes[1] = 0;
    /* make stdin inaccessible */
    xx = pipe(pipes);
    xx = close(pipes[1]);
    xx = dup2(pipes[0], STDIN_FILENO);
    xx = close(pipes[0]);

    /* XXX Force FD_CLOEXEC on all inherited fdno's. */
    open_max = sysconf(_SC_OPEN_MAX);
    if (open_max == -1) {
	open_max = 1024;
    }
    for (fdno = 3; fdno < open_max; fdno++) {
	flag = fcntl(fdno, F_GETFD);
	if (flag == -1 || (flag & FD_CLOEXEC))
	    continue;
	xx = fcntl(fdno, F_SETFD, FD_CLOEXEC);
	/* XXX W2DO? debug msg for inheirited fdno w/o FD_CLOEXEC */
    }

    if (scriptFd != NULL) {
	int sfdno = Fileno(scriptFd);
	int ofdno = Fileno(out);
	if (sfdno != STDERR_FILENO)
	    xx = dup2(sfdno, STDERR_FILENO);
	if (ofdno != STDOUT_FILENO)
	    xx = dup2(ofdno, STDOUT_FILENO);
	/* make sure we don't close stdin/stderr/stdout by mistake! */
	if (ofdno > STDERR_FILENO && ofdno != sfdno)
	    xx = Fclose (out);
	if (sfdno > STDERR_FILENO && ofdno != sfdno)
	    xx = Fclose (scriptFd);
    }

    {   char *ipath = rpmExpand("%{_install_script_path}", NULL);
	const char *path = SCRIPT_PATH;

	if (ipath && ipath[5] != '%')
	    path = ipath;

	xx = setenv("PATH", path, 1);
	ipath = _free(ipath);
    }

    for (ARGV_const_t pf = prefixes; pf && *pf; pf++) {
	char *name = NULL;
	int num = (pf - prefixes);

	rasprintf(&name, "RPM_INSTALL_PREFIX%d", num);
	setenv(name, *pf, 1);
	free(name);

	/* scripts might still be using the old style prefix */
	if (num == 0) {
	    setenv("RPM_INSTALL_PREFIX", *pf, 1);
	}
    }
	
    if (chdir("/") == 0) {
	/* XXX Don't mtrace into children. */
	unsetenv("MALLOC_CHECK_");

	/* Permit libselinux to do the scriptlet exec. */
	if (selinux == 1) {	
	    xx = rpm_execcon(0, argv[0], argv, environ);
	}

	if (xx == 0) {
	    xx = execv(argv[0], argv);
	}
    }
    _exit(127); /* exit 127 for compatibility with bash(1) */
}

static char * writeScript(const char *cmd, const char *script)
{
    char *fn = NULL;
    size_t slen = strlen(script);
    int ok = 0;
    FD_t fd = rpmMkTempFile("/", &fn);

    if (Ferror(fd))
	goto exit;

    if (rpmIsDebug() && (rstreq(cmd, "/bin/sh") || rstreq(cmd, "/bin/bash"))) {
	static const char set_x[] = "set -x\n";
	/* Assume failures will be caught by the write below */
	Fwrite(set_x, sizeof(set_x[0]), sizeof(set_x)-1, fd);
    }

    ok = (Fwrite(script, sizeof(script[0]), slen, fd) == slen);

exit:
    if (!ok) fn = _free(fn);
    Fclose(fd);
    return fn;
}

/**
 * Run an external script.
 */
static rpmRC runExtScript(int selinux, ARGV_const_t prefixes,
		   const char *sname, rpmlogLvl lvl, FD_t scriptFd,
		   ARGV_t * argvp, const char *script, int arg1, int arg2)
{
    FD_t out = NULL;
    char * fn = NULL;
    int xx;
    rpmRC rc = RPMRC_FAIL;
    struct rpmsqElem sq;

    memset(&sq, 0, sizeof(sq));
    sq.reaper = 1;

    rpmlog(RPMLOG_DEBUG, "%s: scriptlet start\n", sname);

    if (script) {
	fn = writeScript(*argvp[0], script);
	if (fn == NULL) {
	    rpmlog(RPMLOG_ERR,
		   _("Couldn't create temporary file for %s: %s\n"),
		   sname, strerror(errno));
	    goto exit;
	}

	argvAdd(argvp, fn);
	if (arg1 >= 0) {
	    argvAddNum(argvp, arg1);
	}
	if (arg2 >= 0) {
	    argvAddNum(argvp, arg2);
	}
    }

    if (scriptFd != NULL) {
	if (rpmIsVerbose()) {
	    out = fdDup(Fileno(scriptFd));
	} else {
	    out = Fopen("/dev/null", "w.fdio");
	    if (Ferror(out)) {
		out = fdDup(Fileno(scriptFd));
	    }
	}
    } else {
	out = fdDup(STDOUT_FILENO);
    }
    if (out == NULL) { 
	rpmlog(RPMLOG_ERR, _("Couldn't duplicate file descriptor: %s: %s\n"),
	       sname, strerror(errno));
	goto exit;
    }

    xx = rpmsqFork(&sq);
    if (sq.child == 0) {
	rpmlog(RPMLOG_DEBUG, "%s: execv(%s) pid %d\n",
	       sname, *argvp[0], (unsigned)getpid());
	doScriptExec(selinux, *argvp, prefixes, scriptFd, out);
    }

    if (sq.child == (pid_t)-1) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"), sname, strerror(errno));
	goto exit;
    }

    rpmsqWait(&sq);

    rpmlog(RPMLOG_DEBUG, "%s: waitpid(%d) rc %d status %x\n",
	   sname, (unsigned)sq.child, (unsigned)sq.reaped, sq.status);

    if (sq.reaped < 0) {
	rpmlog(lvl, _("%s scriptlet failed, waitpid(%d) rc %d: %s\n"),
		 sname, sq.child, sq.reaped, strerror(errno));
    } else if (!WIFEXITED(sq.status) || WEXITSTATUS(sq.status)) {
      	if (WIFSIGNALED(sq.status)) {
	    rpmlog(lvl, _("%s scriptlet failed, signal %d\n"),
                   sname, WTERMSIG(sq.status));
	} else {
	    rpmlog(lvl, _("%s scriptlet failed, exit status %d\n"),
		   sname, WEXITSTATUS(sq.status));
	}
    } else {
	/* if we get this far we're clear */
	rc = RPMRC_OK;
    }

exit:
    if (out)
	xx = Fclose(out);	/* XXX dup'd STDOUT_FILENO */

    if (fn) {
	if (!rpmIsDebug())
	    xx = unlink(fn);
	fn = _free(fn);
    }
    return rc;
}

rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2, FD_t scriptFd,
		   ARGV_const_t prefixes, int warn_only, int selinux)
{
    ARGV_t args = NULL;
    rpmlogLvl lvl = warn_only ? RPMLOG_WARNING : RPMLOG_ERR;
    rpmRC rc;

    if (script == NULL) return RPMRC_OK;

    /* construct a new argv as we can't modify the one from header */
    if (script->args) {
	argvAppend(&args, script->args);
    } else {
	argvAdd(&args, "/bin/sh");
    }

    if (rstreq(args[0], "<lua>")) {
	rc = runLuaScript(selinux, prefixes, script->descr, lvl, scriptFd, &args, script->body, arg1, arg2);
    } else {
	rc = runExtScript(selinux, prefixes, script->descr, lvl, scriptFd, &args, script->body, arg1, arg2);
    }
    argvFree(args);

    return rc;
}

static rpmTagVal getProgTag(rpmTagVal scriptTag)
{
    switch (scriptTag) {
    case RPMTAG_PREIN:		return RPMTAG_PREINPROG;
    case RPMTAG_POSTIN: 	return RPMTAG_POSTINPROG;
    case RPMTAG_PREUN: 		return RPMTAG_PREUNPROG;
    case RPMTAG_POSTUN: 	return RPMTAG_POSTUNPROG;
    case RPMTAG_PRETRANS: 	return RPMTAG_PRETRANSPROG;
    case RPMTAG_POSTTRANS:	return RPMTAG_POSTTRANSPROG;
    case RPMTAG_VERIFYSCRIPT:	return RPMTAG_VERIFYSCRIPTPROG;
    default:			return RPMTAG_NOT_FOUND;
    }
}

static rpmTagVal getFlagTag(rpmTagVal scriptTag)
{
    switch (scriptTag) {
    case RPMTAG_PRETRANS:	return RPMTAG_PRETRANSFLAGS;
    case RPMTAG_POSTTRANS:	return RPMTAG_POSTTRANSFLAGS;
    case RPMTAG_PREUN:		return RPMTAG_PREUNFLAGS;
    case RPMTAG_POSTUN:		return RPMTAG_POSTUNFLAGS;
    case RPMTAG_PREIN:		return RPMTAG_PREINFLAGS;
    case RPMTAG_POSTIN:		return RPMTAG_POSTINFLAGS;
    case RPMTAG_VERIFYSCRIPT:	return RPMTAG_VERIFYSCRIPTFLAGS;
    case RPMTAG_TRIGGERSCRIPTS:	return RPMTAG_TRIGGERSCRIPTFLAGS;
    default:
       break;
    }
    return RPMTAG_NOT_FOUND;
}

static const char * tag2sln(rpmTagVal tag)
{
    switch (tag) {
    case RPMTAG_PRETRANS:       return "%pretrans";
    case RPMTAG_TRIGGERPREIN:   return "%triggerprein";
    case RPMTAG_PREIN:          return "%pre";
    case RPMTAG_POSTIN:         return "%post";
    case RPMTAG_TRIGGERIN:      return "%triggerin";
    case RPMTAG_TRIGGERUN:      return "%triggerun";
    case RPMTAG_PREUN:          return "%preun";
    case RPMTAG_POSTUN:         return "%postun";
    case RPMTAG_POSTTRANS:      return "%posttrans";
    case RPMTAG_TRIGGERPOSTUN:  return "%triggerpostun";
    case RPMTAG_VERIFYSCRIPT:   return "%verify";
    default: break;
    }
    return "%unknownscript";
}

rpmScript rpmScriptFromTag(Header h, rpmTagVal scriptTag)
{
    rpmScript script = NULL;
    rpmTagVal progTag = getProgTag(scriptTag);

    if (headerIsEntry(h, scriptTag) || headerIsEntry(h, progTag)) {
	struct rpmtd_s prog;
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	rpmscriptFlags flags = headerGetNumber(h, getFlagTag(scriptTag));

	script = xcalloc(1, sizeof(*script));
	script->tag = scriptTag;
	rasprintf(&script->descr, "%s(%s)", tag2sln(scriptTag), nevra);
	script->body = headerGetAsString(h, scriptTag);

	/* macros need to be expanded before possible queryformat */
	if (script->body && (flags & RPMSCRIPT_EXPAND)) {
	    char *body = rpmExpand(script->body, NULL);
	    free(script->body);
	    script->body = body;
	}
	if (script->body && (flags & RPMSCRIPT_QFORMAT)) {
	    /* XXX TODO: handle queryformat errors */
	    char *body = headerFormat(h, script->body, NULL);
	    free(script->body);
	    script->body = body;
	}

	if (headerGet(h, progTag, &prog, (HEADERGET_ALLOC|HEADERGET_ARGV))) {
	    script->args = prog.data;
	}
	free(nevra);
    }
    return script;
}

rpmScript rpmScriptFree(rpmScript script)
{
    if (script) {
	free(script->args);
	free(script->body);
	free(script->descr);
	free(script);
    }
    return NULL;
}
