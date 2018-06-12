#include "system.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmio.h>
#include <rpm/rpmlog.h>
#include <rpm/header.h>
#include <rpm/rpmds.h>

#include "rpmio/rpmlua.h"
#include "lib/rpmscript.h"
#include "rpmio/rpmio_internal.h"

#include "lib/rpmplugins.h"     /* rpm plugins hooks */

#include "debug.h"

struct scriptNextFileFunc_s {
    char *(*func)(void *);	/* function producing input for script */
    void *param;		/* parameter for func */
};

typedef struct scriptNextFileFunc_s *scriptNextFileFunc;

struct rpmScript_s {
    rpmscriptTypes type;	/* script type */
    rpmTagVal tag;		/* script tag */
    char **args;		/* scriptlet call arguments */
    char *body;			/* script body */
    char *descr;		/* description for logging */
    rpmscriptFlags flags;	/* flags to control operation */
    struct scriptNextFileFunc_s nextFileFunc;  /* input function */
};

struct scriptInfo_s {
    rpmscriptTypes type;
    const char *desc;
    rpmsenseFlags sense;
    rpmTagVal tag;
    rpmTagVal progtag;
    rpmTagVal flagtag;
    rpmscriptFlags deflags;
};

static const struct scriptInfo_s scriptInfo[] = {
    { RPMSCRIPT_PREIN, "%prein", 0,
	RPMTAG_PREIN, RPMTAG_PREINPROG, RPMTAG_PREINFLAGS,
	RPMSCRIPT_FLAG_CRITICAL, },
    { RPMSCRIPT_PREUN, "%preun", 0,
	RPMTAG_PREUN, RPMTAG_PREUNPROG, RPMTAG_PREUNFLAGS,
	RPMSCRIPT_FLAG_CRITICAL, },
    { RPMSCRIPT_POSTIN, "%post", 0,
	RPMTAG_POSTIN, RPMTAG_POSTINPROG, RPMTAG_POSTINFLAGS,
	0, },
    { RPMSCRIPT_POSTUN, "%postun", 0,
	RPMTAG_POSTUN, RPMTAG_POSTUNPROG, RPMTAG_POSTUNFLAGS,
	0, },
    { RPMSCRIPT_PRETRANS, "%pretrans", 0,
	RPMTAG_PRETRANS, RPMTAG_PRETRANSPROG, RPMTAG_PRETRANSFLAGS,
	RPMSCRIPT_FLAG_CRITICAL, },
    { RPMSCRIPT_POSTTRANS, "%posttrans", 0,
	RPMTAG_POSTTRANS, RPMTAG_POSTTRANSPROG, RPMTAG_POSTTRANSFLAGS,
	0, },
    { RPMSCRIPT_TRIGGERPREIN, "%triggerprein", RPMSENSE_TRIGGERPREIN,
	RPMTAG_TRIGGERPREIN, 0, 0,
	0, },
    { RPMSCRIPT_TRIGGERUN, "%triggerun", RPMSENSE_TRIGGERUN,
	RPMTAG_TRIGGERUN, 0, 0,
	0, },
    { RPMSCRIPT_TRIGGERIN, "%triggerin", RPMSENSE_TRIGGERIN,
	RPMTAG_TRIGGERIN, 0, 0,
	0, },
    { RPMSCRIPT_TRIGGERPOSTUN, "%triggerpostun", RPMSENSE_TRIGGERPOSTUN,
	RPMTAG_TRIGGERPOSTUN, 0, 0,
	0, },
    { RPMSCRIPT_VERIFY, "%verify", 0,
	RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG, RPMTAG_VERIFYSCRIPTFLAGS,
	RPMSCRIPT_FLAG_CRITICAL, },
    { 0, "unknown", 0,
	RPMTAG_NOT_FOUND, RPMTAG_NOT_FOUND, RPMTAG_NOT_FOUND,
	0, }
};

static const struct scriptInfo_s * findTag(rpmTagVal tag)
{
    const struct scriptInfo_s * si = scriptInfo;
    while (si->type && si->tag != tag)
	si++;
    return si;
}
/**
 * Run internal Lua script.
 */
static rpmRC runLuaScript(rpmPlugins plugins, ARGV_const_t prefixes,
		   const char *sname, rpmlogLvl lvl, FD_t scriptFd,
		   ARGV_t * argvp, const char *script, int arg1, int arg2,
		   scriptNextFileFunc nextFileFunc)
{
    rpmRC rc = RPMRC_FAIL;
#ifdef WITH_LUA
    ARGV_t argv = argvp ? *argvp : NULL;
    rpmlua lua = NULL; /* Global state. */
    rpmluav var = rpmluavNew();
    int cwd = -1;

    rpmlog(RPMLOG_DEBUG, "%s: running <lua> scriptlet.\n", sname);

    /* Create arg variable */
    rpmluaPushTable(lua, "arg");
    rpmluavSetListMode(var, 1);
    rpmluaSetNextFileFunc(nextFileFunc->func, nextFileFunc->param);
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
    rpmluaPop(lua);

    /* Lua scripts can change our cwd and umask, save and restore */
    /* XXX TODO: use cwd from chroot state to save unnecessary open here */
    cwd = open(".", O_RDONLY);
    if (cwd != -1) {
	mode_t oldmask = umask(0);
	umask(oldmask);
	pid_t pid = getpid();

	if (chdir("/") == 0 && rpmluaRunScript(lua, script, sname) == 0) {
	    rc = RPMRC_OK;
	}
	if (pid != getpid()) {
	    /* Terminate child process forked in lua scriptlet */
	    rpmlog(RPMLOG_ERR, _("No exec() called after fork() in lua scriptlet\n"));
	    _exit(EXIT_FAILURE);
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
    rpmluavFree(var);

#else
    rpmlog(lvl, _("<lua> scriptlet support not built in\n"));
#endif

    return rc;
}

static const char * const SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

static void doScriptExec(ARGV_const_t argv, ARGV_const_t prefixes,
			FD_t scriptFd, FD_t out)
{
    int xx;

    /* SIGPIPE is ignored in rpm, reset to default for the scriptlet */
    (void) signal(SIGPIPE, SIG_DFL);

    rpmSetCloseOnExec();

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
	free(ipath);
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

	xx = execv(argv[0], argv);
	if (xx) {
	    rpmlog(RPMLOG_ERR,
		    _("failed to exec scriptlet interpreter %s: %s\n"),
		    argv[0], strerror(errno));
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
static rpmRC runExtScript(rpmPlugins plugins, ARGV_const_t prefixes,
		   const char *sname, rpmlogLvl lvl, FD_t scriptFd,
		   ARGV_t * argvp, const char *script, int arg1, int arg2,
		   scriptNextFileFunc nextFileFunc)
{
    FD_t out = NULL;
    char * fn = NULL;
    pid_t pid, reaped;
    int status;
    int inpipe[2];
    FILE *in = NULL;
    const char *line;
    char *mline = NULL;
    rpmRC rc = RPMRC_FAIL;

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

    if (pipe(inpipe) < 0) {
	rpmlog(RPMLOG_ERR,
		("Couldn't create pipe: %s\n"), strerror(errno));
	goto exit;
    }
    in = fdopen(inpipe[1], "w");
    inpipe[1] = 0;

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

    pid = fork();
    if (pid == (pid_t) -1) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"),
		sname, strerror(errno));
	goto exit;
    } else if (pid == 0) {/* Child */
	rpmlog(RPMLOG_DEBUG, "%s: execv(%s) pid %d\n",
	       sname, *argvp[0], (unsigned)getpid());

	fclose(in);
	dup2(inpipe[0], STDIN_FILENO);

	/* Run scriptlet post fork hook for all plugins */
	if (rpmpluginsCallScriptletForkPost(plugins, *argvp[0], RPMSCRIPTLET_FORK | RPMSCRIPTLET_EXEC) != RPMRC_FAIL) {
	    doScriptExec(*argvp, prefixes, scriptFd, out);
	} else {
	    _exit(126); /* exit 126 for compatibility with bash(1) */
	}
    }
    close(inpipe[0]);
    inpipe[0] = 0;

    if (nextFileFunc->func) {
	while ((line = nextFileFunc->func(nextFileFunc->param)) != NULL) {
	    size_t size = strlen(line);
	    size_t ret_size;
	    mline = xstrdup(line);
	    mline[size] = '\n';

	    ret_size = fwrite(mline, size + 1, 1, in);
	    mline = _free(mline);
	    if (ret_size != 1) {
		if (errno == EPIPE) {
		    break;
		} else {
		    rpmlog(RPMLOG_ERR, _("Fwrite failed: %s"), strerror(errno));
		    rc = RPMRC_FAIL;
		    goto exit;
		}
	    }
	}
    }
    fclose(in);
    in = NULL;

    do {
	reaped = waitpid(pid, &status, 0);
    } while (reaped == -1 && errno == EINTR);

    rpmlog(RPMLOG_DEBUG, "%s: waitpid(%d) rc %d status %x\n",
	   sname, (unsigned)pid, (unsigned)reaped, status);

    if (reaped < 0) {
	rpmlog(lvl, _("%s scriptlet failed, waitpid(%d) rc %d: %s\n"),
		 sname, pid, reaped, strerror(errno));
    } else if (!WIFEXITED(status) || WEXITSTATUS(status)) {
      	if (WIFSIGNALED(status)) {
	    rpmlog(lvl, _("%s scriptlet failed, signal %d\n"),
                   sname, WTERMSIG(status));
	} else {
	    rpmlog(lvl, _("%s scriptlet failed, exit status %d\n"),
		   sname, WEXITSTATUS(status));
	}
    } else {
	/* if we get this far we're clear */
	rc = RPMRC_OK;
    }

exit:
    if (in)
	fclose(in);

    if (inpipe[0])
	close(inpipe[0]);

    if (out)
	Fclose(out);	/* XXX dup'd STDOUT_FILENO */

    if (fn) {
	if (!rpmIsDebug())
	    unlink(fn);
	free(fn);
    }
    free(mline);

    return rc;
}

rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2, FD_t scriptFd,
		   ARGV_const_t prefixes, rpmPlugins plugins)
{
    if (script == NULL) return RPMRC_OK;

    ARGV_t args = NULL;
    rpmlogLvl lvl = (script->flags & RPMSCRIPT_FLAG_CRITICAL) ?
		    RPMLOG_ERR : RPMLOG_WARNING;
    rpmRC rc;
    int script_type = RPMSCRIPTLET_FORK | RPMSCRIPTLET_EXEC;

    /* construct a new argv as we can't modify the one from header */
    if (script->args) {
	argvAppend(&args, script->args);
    } else {
	argvAdd(&args, "/bin/sh");
    }
    
    if (rstreq(args[0], "<lua>"))
	script_type = RPMSCRIPTLET_NONE;

    /* Run scriptlet pre hook for all plugins */
    rc = rpmpluginsCallScriptletPre(plugins, script->descr, script_type);

    if (rc != RPMRC_FAIL) {
	if (script_type & RPMSCRIPTLET_EXEC) {
	    rc = runExtScript(plugins, prefixes, script->descr, lvl, scriptFd, &args, script->body, arg1, arg2, &script->nextFileFunc);
	} else {
	    rc = runLuaScript(plugins, prefixes, script->descr, lvl, scriptFd, &args, script->body, arg1, arg2, &script->nextFileFunc);
	}
    }

    /* Run scriptlet post hook for all plugins */
    rpmpluginsCallScriptletPost(plugins, script->descr, script_type, rc);

    argvFree(args);

    return rc;
}

static rpmscriptTypes getScriptType(rpmTagVal scriptTag)
{
    return findTag(scriptTag)->type;
}

static rpmTagVal getProgTag(rpmTagVal scriptTag)
{
    return findTag(scriptTag)->progtag;
}

static rpmTagVal getFlagTag(rpmTagVal scriptTag)
{
    return findTag(scriptTag)->flagtag;
}

static rpmscriptFlags getDefFlags(rpmTagVal scriptTag)
{
    return findTag(scriptTag)->deflags;
}

static const char * tag2sln(rpmTagVal tag)
{
    return findTag(tag)->desc;
}

static rpmScript rpmScriptNew(Header h, rpmTagVal tag, const char *body,
			      rpmscriptFlags flags)
{
    char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
    rpmScript script = xcalloc(1, sizeof(*script));
    script->tag = tag;
    script->type = getScriptType(tag);
    script->flags = getDefFlags(tag) | flags;
    script->body = (body != NULL) ? xstrdup(body) : NULL;
    rasprintf(&script->descr, "%s(%s)", tag2sln(tag), nevra);

    /* macros need to be expanded before possible queryformat */
    if (script->body && (script->flags & RPMSCRIPT_FLAG_EXPAND)) {
	char *body = rpmExpand(script->body, NULL);
	free(script->body);
	script->body = body;
    }
    if (script->body && (script->flags & RPMSCRIPT_FLAG_QFORMAT)) {
	/* XXX TODO: handle queryformat errors */
	char *body = headerFormat(h, script->body, NULL);
	free(script->body);
	script->body = body;
    }

    script->nextFileFunc.func = NULL;
    script->nextFileFunc.param = NULL;

    free(nevra);
    return script;
}

void rpmScriptSetNextFileFunc(rpmScript script, char *(*func)(void *),
			    void *param)
{
    script->nextFileFunc.func = func;
    script->nextFileFunc.param = param;
}

rpmTagVal triggerDsTag(rpmscriptTriggerModes tm)
{
    rpmTagVal tag = RPMTAG_NOT_FOUND;
    switch (tm) {
    case RPMSCRIPT_NORMALTRIGGER:
	tag = RPMTAG_TRIGGERNAME;
	break;
    case RPMSCRIPT_FILETRIGGER:
	tag = RPMTAG_FILETRIGGERNAME;
	break;
    case RPMSCRIPT_TRANSFILETRIGGER:
	tag = RPMTAG_TRANSFILETRIGGERNAME;
	break;
    }
    return tag;
}

rpmscriptTriggerModes triggerMode(rpmTagVal tag)
{
    rpmscriptTriggerModes tm = 0;
    switch (tag) {
    case RPMTAG_TRIGGERNAME:
	tm = RPMSCRIPT_NORMALTRIGGER;
	break;
    case RPMTAG_FILETRIGGERNAME:
	tm = RPMSCRIPT_FILETRIGGER;
	break;
    case RPMTAG_TRANSFILETRIGGERNAME:
	tm = RPMSCRIPT_TRANSFILETRIGGER;
	break;
    }
    return tm;
}

rpmTagVal triggertag(rpmsenseFlags sense)
{
    rpmTagVal tag = RPMTAG_NOT_FOUND;
    switch (sense) {
    case RPMSENSE_TRIGGERIN:
	tag = RPMTAG_TRIGGERIN;
	break;
    case RPMSENSE_TRIGGERUN:
	tag = RPMTAG_TRIGGERUN;
	break;
    case RPMSENSE_TRIGGERPOSTUN:
	tag = RPMTAG_TRIGGERPOSTUN;
	break;
    case RPMSENSE_TRIGGERPREIN:
	tag = RPMTAG_TRIGGERPREIN;
	break;
    default:
	break;
    }
    return tag;
}

rpmScript rpmScriptFromTriggerTag(Header h, rpmTagVal triggerTag,
			    rpmscriptTriggerModes tm, uint32_t ix)
{
    rpmScript script = NULL;
    struct rpmtd_s tscripts, tprogs, tflags;
    headerGetFlags hgflags = HEADERGET_MINMEM;

    switch (tm) {
	case RPMSCRIPT_NORMALTRIGGER:
	    headerGet(h, RPMTAG_TRIGGERSCRIPTS, &tscripts, hgflags);
	    headerGet(h, RPMTAG_TRIGGERSCRIPTPROG, &tprogs, hgflags);
	    headerGet(h, RPMTAG_TRIGGERSCRIPTFLAGS, &tflags, hgflags);
	    break;
	case RPMSCRIPT_FILETRIGGER:
	    headerGet(h, RPMTAG_FILETRIGGERSCRIPTS, &tscripts, hgflags);
	    headerGet(h, RPMTAG_FILETRIGGERSCRIPTPROG, &tprogs, hgflags);
	    headerGet(h, RPMTAG_FILETRIGGERSCRIPTFLAGS, &tflags, hgflags);
	    break;
	case RPMSCRIPT_TRANSFILETRIGGER:
	    headerGet(h, RPMTAG_TRANSFILETRIGGERSCRIPTS, &tscripts, hgflags);
	    headerGet(h, RPMTAG_TRANSFILETRIGGERSCRIPTPROG, &tprogs, hgflags);
	    headerGet(h, RPMTAG_TRANSFILETRIGGERSCRIPTFLAGS, &tflags, hgflags);
	    break;
    }

    if (rpmtdSetIndex(&tscripts, ix) >= 0 && rpmtdSetIndex(&tprogs, ix) >= 0) {
	rpmscriptFlags sflags = 0;
	const char *prog = rpmtdGetString(&tprogs);

	if (rpmtdSetIndex(&tflags, ix) >= 0)
	    sflags = rpmtdGetNumber(&tflags);

	script = rpmScriptNew(h, triggerTag, rpmtdGetString(&tscripts), sflags);

	/* hack up a hge-style NULL-terminated array */
	script->args = xmalloc(2 * sizeof(*script->args) + strlen(prog) + 1);
	script->args[0] = (char *)(script->args + 2);
	script->args[1] = NULL;
	strcpy(script->args[0], prog);
    }

    rpmtdFreeData(&tscripts);
    rpmtdFreeData(&tprogs);
    rpmtdFreeData(&tflags);

    return script;
}

rpmScript rpmScriptFromTag(Header h, rpmTagVal scriptTag)
{
    rpmScript script = NULL;
    rpmTagVal progTag = getProgTag(scriptTag);

    if (headerIsEntry(h, scriptTag) || headerIsEntry(h, progTag)) {
	struct rpmtd_s prog;

	script = rpmScriptNew(h, scriptTag,
			      headerGetString(h, scriptTag),
			      headerGetNumber(h, getFlagTag(scriptTag)));

	if (headerGet(h, progTag, &prog, (HEADERGET_ALLOC|HEADERGET_ARGV))) {
	    script->args = prog.data;
	}
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

rpmTagVal rpmScriptTag(rpmScript script)
{
    return (script != NULL) ? script->tag : RPMTAG_NOT_FOUND;
}

rpmscriptTypes rpmScriptType(rpmScript script)
{
    return (script != NULL) ? script->type : 0;
}

rpmscriptFlags rpmScriptFlags(rpmScript script)
{
    return (script != NULL) ? script->flags : 0;
}
