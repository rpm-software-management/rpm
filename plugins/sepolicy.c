#include "plugin.h"

#if WITH_SELINUX

#include <errno.h>
#include <selinux/selinux.h>
#include <semanage/semanage.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <rpm/rpmpol.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>

#include "rpmio/base64.h"
#include "lib/rpmte_internal.h"
#include "lib/rpmts_internal.h"	/* rpmtsSELabelFoo() */

rpmPluginHook PLUGIN_HOOKS = \
	PLUGINHOOK_INIT | \
	PLUGINHOOK_CLEANUP | \
	PLUGINHOOK_OPENTE | \
	PLUGINHOOK_COLL_POST_ADD | \
	PLUGINHOOK_COLL_PRE_REMOVE;

typedef enum sepolAction {
    SEPOL_ACTION_IGNORE,
    SEPOL_ACTION_INSTALL,
    SEPOL_ACTION_REMOVE
} sepolAction;

typedef struct sepol {
    char *data;			/*!< policy data */
    char *name;			/*!< policy names */
    ARGV_t types;		/*!< policy types */
    uint32_t flags;		/*!< policy flags */
    sepolAction action;		/*!< install/remove/ignore */
    struct sepol *next;		/*!< next in linked list */
} sepol;

typedef struct sepoltrans {
    int execsemodule;		/*!< 0 = use libsemanage to install policy; non-zero = use semodule */
    semanage_handle_t *sh;	/*!< handle to libsemanage, only used when execsemodule is zero */
    char *semodulepath;		/*!< path to semodule binary */
    ARGV_t semodargs;		/*!< argument list to pass to semodule, only used when execsemodule is non-zero */
    ARGV_t filelist;		/*!< list of temporary files that have been written to disk during the transaction */
    int changes;		/*!< number of changes made during the transaction */
} sepoltrans;


static char * name;
static rpmts ts;

static sepol * policiesHead;
static sepol * policiesTail;


static sepol *sepolNew(rpmte te);
static sepol *sepolFree(sepol * pol);
static int sepolHasType(const sepol * pol, const char *type);

static rpmRC sepolPreparePolicies(sepol * pols, const char *policytype);
static rpmRC sepolWritePolicy(const sepol * pol, char **path);
static rpmRC sepolLoadPolicies(const sepol * pols);

static sepoltrans *sepoltransNew(void);
static sepoltrans *sepoltransFree(sepoltrans * pt);

static rpmRC sepoltransInstall(sepoltrans * pt, const sepol * pol);
static rpmRC sepoltransRemove(sepoltrans * pt, const sepol * pol);
static rpmRC sepoltransCommit(sepoltrans * pt);


static sepol *sepolNew(rpmte te)
{
    sepol *head = NULL;
    sepol *ret = NULL;
    sepolAction action;
    Header h;
    struct rpmtd_s policies, names, types, typesidx, flags;
    int i, j;
    int count;

    rpmtdReset(&policies);
    rpmtdReset(&names);
    rpmtdReset(&types);
    rpmtdReset(&typesidx);
    rpmtdReset(&flags);

    h = rpmteHeader(te);
    if (!h) {
	goto exit;
    }

    if (!headerIsEntry(h, RPMTAG_POLICIES)) {
	goto exit;
    }

    if (!headerGet(h, RPMTAG_POLICIES, &policies, HEADERGET_MINMEM)) {
	goto exit;
    }

    count = rpmtdCount(&policies);
    if (count <= 0) {
	goto exit;
    }

    if (!headerGet(h, RPMTAG_POLICYNAMES, &names, HEADERGET_MINMEM)
	|| rpmtdCount(&names) != count) {
	goto exit;
    }

    if (!headerGet(h, RPMTAG_POLICYFLAGS, &flags, HEADERGET_MINMEM)
	|| rpmtdCount(&flags) != count) {
	goto exit;
    }

    if (!headerGet(h, RPMTAG_POLICYTYPES, &types, HEADERGET_MINMEM)) {
	goto exit;
    }

    if (!headerGet(h, RPMTAG_POLICYTYPESINDEXES, &typesidx, HEADERGET_MINMEM)
	|| rpmtdCount(&types) != rpmtdCount(&typesidx)) {
	goto exit;
    }

    action = (rpmteType(te) == TR_ADDED) ? SEPOL_ACTION_INSTALL : SEPOL_ACTION_REMOVE;

    for (i = 0; i < count; i++) {
	sepol *pol = xcalloc(1, sizeof(*pol));
	pol->next = head;
	head = pol;

	pol->data = xstrdup(rpmtdNextString(&policies));
	pol->name = xstrdup(rpmtdNextString(&names));
	pol->flags = *rpmtdNextUint32(&flags);
	pol->action = action;

	for (j = 0; j < rpmtdCount(&types); j++) {
	    uint32_t index = ((uint32_t *) typesidx.data)[j];
	    if (index < 0 || index >= count) {
		goto exit;
	    }
	    if (index != i) {
		continue;
	    }
	    argvAdd(&pol->types, rpmtdNextString(&types));
	}
	argvSort(pol->types, NULL);
    }

    ret = head;

  exit:
    headerFree(h);

    rpmtdFreeData(&policies);
    rpmtdFreeData(&names);
    rpmtdFreeData(&types);
    rpmtdFreeData(&typesidx);
    rpmtdFreeData(&flags);

    if (!ret) {
	sepolFree(head);
    }

    return ret;
}

static sepol *sepolFree(sepol * pol)
{
    while (pol) {
	sepol *next = pol->next;

	pol->data = _free(pol->data);
	pol->name = _free(pol->name);
	pol->types = argvFree(pol->types);
	pol->next = NULL;
	_free(pol);

	pol = next;
    }

    return NULL;
}

int sepolHasType(const sepol * pol, const char *type)
{
    if (!pol || !type) {
	return 0;
    }

    return (argvSearch(pol->types, type, NULL) != NULL) ||
	   (argvSearch(pol->types, RPMPOL_TYPE_DEFAULT, NULL) != NULL);
}

static rpmRC sepolPreparePolicies(sepol * pols, const char *policytype)
{
    sepol *pol;
    rpmRC rc = RPMRC_OK;

    for (pol = pols; pol; pol = pol->next) {
	if (!sepolHasType(pol, policytype)) {
	    pol->action = SEPOL_ACTION_IGNORE;
	}
    }

    return rc;
}

static rpmRC sepolWritePolicy(const sepol * pol, char **path)
{
    char *tmppath = NULL;
    FD_t fd = NULL;
    char *policy = NULL;
    size_t policylen;
    rpmRC rc = RPMRC_FAIL;

    if (b64decode(pol->data, (void **) &policy, &policylen) != 0) {
	rpmlog(RPMLOG_ERR, _("Failed to decode policy for %s\n"),
	       pol->name);
	goto exit;
    }

    fd = rpmMkTempFile(NULL, &tmppath);
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Failed to create temporary file for %s: %s\n"),
	       pol->name, strerror(errno));
	goto exit;
    }

    if (!Fwrite(policy, sizeof(*policy), policylen, fd)) {
	rpmlog(RPMLOG_ERR, _("Failed to write %s policy to file %s\n"),
	       pol->name, tmppath);
	goto exit;
    }

    *path = tmppath;
    rc = RPMRC_OK;

  exit:
    if (fd)
	Fclose(fd);
    _free(policy);
    if (rc != RPMRC_OK)
	_free(tmppath);

    return rc;
}

static rpmRC sepolLoadPolicies(const sepol * pols)
{
    const sepol *pol;
    sepoltrans *pt;
    rpmRC rc = RPMRC_FAIL;

    pt = sepoltransNew();
    if (!pt) {
	rc = RPMRC_FAIL;
	goto err;
    }

    for (pol = pols; pol; pol = pol->next) {
	switch (pol->action) {
	case SEPOL_ACTION_REMOVE:
	    rc = sepoltransRemove(pt, pol);
	    break;
	case SEPOL_ACTION_INSTALL:
	    rc = sepoltransInstall(pt, pol);
	    break;
	case SEPOL_ACTION_IGNORE:
	default:
	    rc = RPMRC_OK;
	    break;
	}

	if (rc != RPMRC_OK) {
	    goto err;
	}
    }

    rc = sepoltransCommit(pt);
    if (rc != RPMRC_OK) {
	goto err;
    }

  err:
    pt = sepoltransFree(pt);

    return rc;
}

static sepoltrans *sepoltransNew(void)
{
    sepoltrans *pt = xcalloc(1, sizeof(*pt));
    pt->semodulepath = rpmExpand("%{__semodule}", NULL);
    pt->execsemodule = (!rpmChrootDone() && access(pt->semodulepath, X_OK) == 0);
    pt->changes = 0;

    if (pt->execsemodule) {
	argvAdd(&pt->semodargs, "semodule");
    } else {
	pt->sh = semanage_handle_create();
	if (!pt->sh) {
	    rpmlog(RPMLOG_ERR, _("Failed to create semanage handle\n"));
	    goto err;
	}
	semanage_set_create_store(pt->sh, 1);
	semanage_set_check_contexts(pt->sh, 0);
	if (semanage_connect(pt->sh) < 0) {
	    rpmlog(RPMLOG_ERR, _("Failed to connect to policy handler\n"));
	    goto err;
	}
	if (semanage_begin_transaction(pt->sh) < 0) {
	    rpmlog(RPMLOG_ERR, _("Failed to begin policy transaction: %s\n"),
		   errno ? strerror(errno) : "");
	    goto err;
	}
	semanage_set_reload(pt->sh, !rpmChrootDone());
    }

    return pt;

  err:
    if (pt->sh) {
	if (semanage_is_connected(pt->sh)) {
	    semanage_disconnect(pt->sh);
	}
	semanage_handle_destroy(pt->sh);
    }
    pt = _free(pt);

    return pt;
}

static sepoltrans *sepoltransFree(sepoltrans * pt)
{
    ARGV_t file;

    if (!pt) {
	return NULL;
    }

    for (file = pt->filelist; file && *file; file++) {
	if (unlink(*file) < 0) {
	    rpmlog(RPMLOG_WARNING, _("Failed to remove temporary policy file %s: %s\n"),
		   *file, strerror(errno));
	}
    }
    argvFree(pt->filelist);

    if (pt->execsemodule) {
	argvFree(pt->semodargs);
    } else {
	semanage_disconnect(pt->sh);
	semanage_handle_destroy(pt->sh);
    }

    pt->semodulepath = _free(pt->semodulepath);

    pt = _free(pt);
    return NULL;
}

static rpmRC sepoltransInstall(sepoltrans * pt, const sepol * pol)
{
    rpmRC rc = RPMRC_OK;
    char *path = NULL;

    rc = sepolWritePolicy(pol, &path);
    if (rc != RPMRC_OK) {
	return rc;
    }
    argvAdd(&pt->filelist, path);

    if (pt->execsemodule) {
	const char *flag = (pol->flags & RPMPOL_FLAG_BASE) ? "-b" : "-i";
	if (argvAdd(&pt->semodargs, flag) < 0 || argvAdd(&pt->semodargs, path) < 0) {
	    rc = RPMRC_FAIL;
	}
    } else {
	if (pol->flags & RPMPOL_FLAG_BASE) {
	    if (semanage_module_install_base_file(pt->sh, path) < 0) {
		rc = RPMRC_FAIL;
	    }
	} else {
	    if (semanage_module_install_file(pt->sh, path) < 0) {
		rc = RPMRC_FAIL;
	    }
	}
    }

    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("Failed to install policy module: %s (%s)\n"),
	       pol->name, path);
    } else {
	pt->changes++;
    }

    _free(path);

    return rc;
}

static rpmRC sepoltransRemove(sepoltrans * pt, const sepol * pol)
{
    rpmRC rc = RPMRC_OK;

    if (pol->flags & RPMPOL_FLAG_BASE) {
	return RPMRC_FAIL;
    }

    if (pt->execsemodule) {
	if (argvAdd(&pt->semodargs, "-r") < 0 || argvAdd(&pt->semodargs, pol->name) < 0) {
	    rc = RPMRC_FAIL;
	}
    } else {
	if (semanage_module_remove(pt->sh, (char *) pol->name) < 0) {
	    rc = RPMRC_FAIL;
	}
    }

    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("Failed to remove policy module: %s\n"),
	       pol->name);
    } else {
	pt->changes++;
    }

    return rc;
}

static rpmRC sepoltransCommit(sepoltrans * pt)
{
    rpmRC rc = RPMRC_OK;

    if (pt->changes == 0) {
	return rc;
    }

    if (pt->execsemodule) {
	int status;
	pid_t pid = fork();
	int fd;

	switch (pid) {
	case -1:
	    rpmlog(RPMLOG_ERR, _("Failed to fork process: %s\n"),
		   strerror(errno));
	    rc = RPMRC_FAIL;
	    break;
	case 0:
	    fd = open("/dev/null", O_RDWR);
	    dup2(fd, STDIN_FILENO);
	    dup2(fd, STDOUT_FILENO);
	    dup2(fd, STDERR_FILENO);
	    execv(pt->semodulepath, pt->semodargs);
	    rpmlog(RPMLOG_ERR, _("Failed to execute %s: %s\n"),
		   pt->semodulepath, strerror(errno));
	    exit(1);
	default:
	    waitpid(pid, &status, 0);
	    if (!WIFEXITED(status)) {
		rpmlog(RPMLOG_ERR, _("%s terminated abnormally\n"),
		       pt->semodulepath);
		rc = RPMRC_FAIL;
	    } else if (WEXITSTATUS(status)) {
		rpmlog(RPMLOG_ERR, _("%s failed with exit code %i\n"),
		       pt->semodulepath, WEXITSTATUS(status));
		rc = RPMRC_FAIL;
	    }
	}
    } else {
	if (semanage_commit(pt->sh) < 0) {
	    rpmlog(RPMLOG_ERR, _("Failed to commit policy changes\n"));
	    rc = RPMRC_FAIL;
	}
    }

    return rc;
}

static rpmRC sepolRelabelFiles(void)
{
    rpmRC rc = RPMRC_OK;
    pid_t pid;
    int fd;
    int status;
    char *restoreconPath = rpmExpand("%{__restorecon}", NULL);

    if (!restoreconPath) {
	rpmlog(RPMLOG_ERR, _("Failed to expand restorecon path"));
	return RPMRC_FAIL;
    }

    /* execute restorecon -R / */
    pid = fork();
    switch (pid) {
    case -1:
	rpmlog(RPMLOG_ERR, _("Failed to fork process: %s\n"),
	       strerror(errno));
	rc = RPMRC_FAIL;
	break;
    case 0:
	fd = open("/dev/null", O_RDWR);
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	execl(restoreconPath, "restorecon", "-R", "/", NULL);
	rpmlog(RPMLOG_ERR, _("Failed to execute %s: %s\n"), restoreconPath,
	       strerror(errno));
	exit(1);
    default:
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status)) {
	    rpmlog(RPMLOG_ERR, _("%s terminated abnormally\n"),
		   restoreconPath);
	    rc = RPMRC_FAIL;
	} else if (WEXITSTATUS(status)) {
	    rpmlog(RPMLOG_ERR, _("%s failed with exit code %i\n"),
		   restoreconPath, WEXITSTATUS(status));
	    rc = RPMRC_FAIL;
	}
    }

    _free(restoreconPath);

    return rc;
}

static rpmRC sepolGo()
{
    semanage_handle_t *sh;
    int existingPolicy;
    char *policytype = NULL;
    rpmRC rc = RPMRC_FAIL;

    static int performed = 0;
    if (performed) {
	return RPMRC_OK;
    }
    performed = 1;

    if (rpmChrootIn()) {
	goto exit;
    }

    if (selinux_getpolicytype(&policytype) < 0) {
	goto exit;
    }

    sepolPreparePolicies(policiesHead, policytype);

    /* determine if this is the first time installing policy */
    sh = semanage_handle_create();
    existingPolicy = (semanage_is_managed(sh) == 1);
    semanage_handle_destroy(sh);

    /* now load the policies */
    rc = sepolLoadPolicies(policiesHead);

    /* re-init selinux and re-read the files contexts, since things may have changed */
    selinux_reset_config();
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	if (rpmtsSELabelInit(ts, selinux_file_context_path()) == RPMRC_OK) {
	    /* if this was the first time installing policy, every package before
	     * policy was installed will be mislabeled (e.g. semodule). So, relabel
	     * the entire filesystem if this is the case */
	    if (!existingPolicy) {
		if (sepolRelabelFiles() != RPMRC_OK) {
		    rpmlog(RPMLOG_WARNING, _("Failed to relabel filesystem. Files may be mislabeled\n"));
		}
	    }
	} else {
	    rpmlog(RPMLOG_WARNING, _("Failed to reload file contexts. Files may be mislabeled\n"));
	}
    }

  exit:
    if (rpmChrootOut()) {
	rc = RPMRC_FAIL;
    }

    _free(policytype);

    return rc;
}

static rpmRC sepolAddTE(rpmte te)
{
    sepol *pol;
    sepol *polTail;

    if (!rpmteHasCollection(te, name)) {
	return RPMRC_OK;
    }

    pol = sepolNew(te);
    if (!pol) {
	/* something's wrong with the policy information, either missing or
	 * corrupt. abort */
	rpmlog(RPMLOG_ERR, _("Failed to extract policy from %s\n"),
	       rpmteNEVRA(te));
	return RPMRC_FAIL;
    }

    /* find the tail of pol */
    polTail = pol;
    while (polTail->next) {
	polTail = polTail->next;
    }

    /* add the new policy to the list */
    if (!policiesHead) {
	policiesHead = pol;
	policiesTail = polTail;
    } else {
	if (rpmteType(te) == TR_ADDED) {
	    /* add to the end of the list */
	    policiesTail->next = pol;
	    policiesTail = polTail;
	} else {
	    /* add to the beginning of the list */
	    polTail->next = policiesHead;
	    policiesHead = pol;
	}
    }

    return RPMRC_OK;
}

#endif	/* WITH_SELINUX */


rpmRC PLUGINHOOK_INIT_FUNC(rpmts _ts, const char *_name, const char *_opts)
{
#if WITH_SELINUX
    ts = _ts;
    name = strdup(_name);
    policiesHead = policiesTail = NULL;
#endif
    return RPMRC_OK;
}

rpmRC PLUGINHOOK_CLEANUP_FUNC(void)
{
#if WITH_SELINUX
    _free(name);
    ts = NULL;
    policiesHead = policiesTail = sepolFree(policiesHead);
#endif
    return RPMRC_OK;
}

rpmRC PLUGINHOOK_OPENTE_FUNC(rpmte te)
{
    rpmRC rc = RPMRC_OK;
#if WITH_SELINUX
    rc = sepolAddTE(te);
#endif
    return rc;
}

rpmRC PLUGINHOOK_COLL_POST_ADD_FUNC(void)
{
    rpmRC rc = RPMRC_OK;
#if WITH_SELINUX
    rc = sepolGo();
#endif
    return rc;
}

rpmRC PLUGINHOOK_COLL_PRE_REMOVE_FUNC(void)
{
    rpmRC rc = RPMRC_OK;
#if WITH_SELINUX
    rc = sepolGo();
#endif
    return rc;
}
