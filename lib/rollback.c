/** \ingroup rpmtrans payload
 * \file lib/rollback.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rollback.h"

#include "debug.h"

/*@access h@*/		/* compared with NULL */
static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)   free((void *)this);
    return NULL;
}

void loadFi(Header h, TFI_t fi)
{
    HGE_t hge;
    uint_32 * uip;
    int len;
    int rc;
    int i;
    
    if (fi->fsm == NULL)
	fi->fsm = newFSM();

    /* XXX avoid gcc noise on pointer (4th arg) cast(s) */
    hge = (fi->type == TR_ADDED)
	? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry;
    fi->hge = hge;

    if (h && fi->h == NULL)	fi->h = headerLink(h);

    /* Duplicate name-version-release so that headers can be free'd. */
    hge(fi->h, RPMTAG_NAME, NULL, (void **) &fi->name, NULL);
    fi->name = xstrdup(fi->name);
    hge(fi->h, RPMTAG_VERSION, NULL, (void **) &fi->version, NULL);
    fi->version = xstrdup(fi->version);
    hge(fi->h, RPMTAG_RELEASE, NULL, (void **) &fi->release, NULL);
    fi->release = xstrdup(fi->release);

    /* -1 means not found */
    rc = hge(fi->h, RPMTAG_EPOCH, NULL, (void **) &uip, NULL);
    fi->epoch = (rc ? *uip : -1);
    /* 0 means unknown */
    rc = hge(fi->h, RPMTAG_ARCHIVESIZE, NULL, (void **) &uip, NULL);
    fi->archiveSize = (rc ? *uip : 0);

    if (!hge(fi->h, RPMTAG_BASENAMES, NULL, (void **) &fi->bnl, &fi->fc)) {
	fi->dc = 0;
	fi->fc = 0;
	return;
    }

    hge(fi->h, RPMTAG_DIRINDEXES, NULL, (void **) &fi->dil, NULL);
    hge(fi->h, RPMTAG_DIRNAMES, NULL, (void **) &fi->dnl, &fi->dc);
    hge(fi->h, RPMTAG_FILEMODES, NULL, (void **) &fi->fmodes, NULL);
    hge(fi->h, RPMTAG_FILEFLAGS, NULL, (void **) &fi->fflags, NULL);
    hge(fi->h, RPMTAG_FILESIZES, NULL, (void **) &fi->fsizes, NULL);
    hge(fi->h, RPMTAG_FILESTATES, NULL, (void **) &fi->fstates, NULL);

    /* actions is initialized earlier for added packages */
    if (fi->actions == NULL)
	fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));

    switch (fi->type) {
    case TR_ADDED:
	hge(fi->h, RPMTAG_FILEMD5S, NULL, (void **) &fi->fmd5s, NULL);
	hge(fi->h, RPMTAG_FILELINKTOS, NULL, (void **) &fi->flinks, NULL);
	hge(fi->h, RPMTAG_FILELANGS, NULL, (void **) &fi->flangs, NULL);

	/* 0 makes for noops */
	fi->replacedSizes = xcalloc(fi->fc, sizeof(*fi->replacedSizes));

	break;
    case TR_REMOVED:
	hge(fi->h, RPMTAG_FILEMD5S, NULL, (void **) &fi->fmd5s, NULL);
	hge(fi->h, RPMTAG_FILELINKTOS, NULL, (void **) &fi->flinks, NULL);
	fi->fsizes = memcpy(xmalloc(fi->fc * sizeof(*fi->fsizes)),
				fi->fsizes, fi->fc * sizeof(*fi->fsizes));
	fi->fflags = memcpy(xmalloc(fi->fc * sizeof(*fi->fflags)),
				fi->fflags, fi->fc * sizeof(*fi->fflags));
	fi->fmodes = memcpy(xmalloc(fi->fc * sizeof(*fi->fmodes)),
				fi->fmodes, fi->fc * sizeof(*fi->fmodes));
	/* XXX there's a tedious segfault here for some version(s) of rpm */
	if (fi->fstates)
	    fi->fstates = memcpy(xmalloc(fi->fc * sizeof(*fi->fstates)),
				fi->fstates, fi->fc * sizeof(*fi->fstates));
	else
	    fi->fstates = xcalloc(1, fi->fc * sizeof(*fi->fstates));
	fi->dil = memcpy(xmalloc(fi->fc * sizeof(*fi->dil)),
				fi->dil, fi->fc * sizeof(*fi->dil));
	headerFree(fi->h);
	fi->h = NULL;
	break;
    }

    fi->dnlmax = -1;
    for (i = 0; i < fi->dc; i++) {
	if ((len = strlen(fi->dnl[i])) > fi->dnlmax)
	    fi->dnlmax = len;
    }

    fi->bnlmax = -1;
    for (i = 0; i < fi->fc; i++) {
	if ((len = strlen(fi->bnl[i])) > fi->bnlmax)
	    fi->bnlmax = len;
    }

    return;
}

void freeFi(TFI_t fi)
{
    fi->name = _free(fi->name);
    fi->version = _free(fi->version);
    fi->release = _free(fi->release);
    fi->actions = _free(fi->actions);
    fi->replacedSizes = _free(fi->replacedSizes);
    fi->replaced = _free(fi->replaced);

    fi->bnl = headerFreeData(fi->bnl, -1);
    fi->dnl = headerFreeData(fi->dnl, -1);
    fi->obnl = headerFreeData(fi->obnl, -1);
    fi->odnl = headerFreeData(fi->odnl, -1);
    fi->flinks = headerFreeData(fi->flinks, -1);
    fi->fmd5s = headerFreeData(fi->fmd5s, -1);
    fi->fuser = headerFreeData(fi->fuser, -1);
    fi->fgroup = headerFreeData(fi->fgroup, -1);
    fi->flangs = headerFreeData(fi->flangs, -1);

#ifdef	HACK_ALERT	/* XXX something's fubar here. */
    fi->apath = _free(fi->apath);
#endif
    fi->fuids = _free(fi->fuids);
    fi->fgids = _free(fi->fgids);
    fi->fmapflags = _free(fi->fmapflags);

    fi->fsm = freeFSM(fi->fsm);

    switch (fi->type) {
    case TR_ADDED:
	    break;
    case TR_REMOVED:
	fi->fsizes = headerFreeData(fi->fsizes, -1);
	fi->fflags = headerFreeData(fi->fflags, -1);
	fi->fmodes = headerFreeData(fi->fmodes, -1);
	fi->fstates = headerFreeData(fi->fstates, -1);
	fi->dil = headerFreeData(fi->dil, -1);
	break;
    }
    if (fi->h) {
	headerFree(fi->h); fi->h = NULL;
    }
}

/*@observer@*/ const char *const fiTypeString(TFI_t fi) {
    switch(fi->type) {
    case TR_ADDED:	return "install";
    case TR_REMOVED:	return "  erase";
    default:		return "???";
    }
    /*@noteached@*/
}

/*@observer@*/ const char *const fileStageString(fileStage a) {
    switch(a) {
    case FSM_UNKNOWN:	return "unknown";
    case FSM_CREATE:	return "create";
    case FSM_INIT:	return "init";
    case FSM_MAP:	return "map";
    case FSM_MKDIRS:	return "mkdirs";
    case FSM_RMDIRS:	return "rmdirs";
    case FSM_PRE:	return "pre";
    case FSM_PROCESS:	return "process";
    case FSM_POST:	return "post";
    case FSM_MKLINKS:	return "mklinks";
    case FSM_NOTIFY:	return "notify";
    case FSM_UNDO:	return "undo";
    case FSM_COMMIT:	return "commit";
    case FSM_FINALIZE:	return "finalize";
    case FSM_DESTROY:	return "destroy";
    case FSM_VERIFY:	return "verify";

    case FSM_UNLINK:	return "unlink";
    case FSM_RENAME:	return "rename";
    case FSM_MKDIR:	return "mkdir";
    case FSM_RMDIR:	return "rmdir";
    case FSM_CHOWN:	return "chown";
    case FSM_LCHOWN:	return "lchown";
    case FSM_CHMOD:	return "chmod";
    case FSM_UTIME:	return "utime";
    case FSM_SYMLINK:	return "symlink";
    case FSM_LINK:	return "link";
    case FSM_MKFIFO:	return "mkfifo";
    case FSM_MKNOD:	return "mknod";
    case FSM_LSTAT:	return "lstat";
    case FSM_STAT:	return "stat";
    case FSM_CHROOT:	return "chroot";

    case FSM_NEXT:	return "next";
    case FSM_EAT:	return "eat";
    case FSM_POS:	return "pos";
    case FSM_PAD:	return "pad";
    default:		return "???";
    }
    /*@noteached@*/
}

/*@obserever@*/ const char *const fileActionString(fileAction a)
{
    switch (a) {
    case FA_UNKNOWN:	return "unknown";
    case FA_CREATE:	return "create";
    case FA_BACKUP:	return "backup";
    case FA_SAVE:	return "save";
    case FA_SKIP:	return "skip";
    case FA_ALTNAME:	return "altname";
    case FA_REMOVE:	return "remove";
    case FA_SKIPNSTATE: return "skipnstate";
    case FA_SKIPNETSHARED: return "skipnetshared";
    case FA_SKIPMULTILIB: return "skipmultilib";
    default:		return "???";
    }
    /*@notreached@*/
}

/**
 */
struct pkgIterator {
/*@dependent@*/ /*@kept@*/ TFI_t fi;
    int i;
};

/**
 */
static /*@null@*/ void * pkgFreeIterator(/*@only@*/ /*@null@*/ void * this) {
    if (this) {
	struct pkgIterator * pi = this;
	TFI_t fi = pi->fi;
    }
    return _free(this);
}

/**
 */
static /*@only@*/ void * pkgInitIterator(/*@kept@*/ rpmTransactionSet ts,
	/*@kept@*/ TFI_t fi)
{
    struct pkgIterator * pi = NULL;
    if (ts && fi) {
	pi = xcalloc(sizeof(*pi), 1);
	pi->fi = fi;
	switch (fi->type) {
	case TR_ADDED:	pi->i = 0;	break;
	case TR_REMOVED:	pi->i = fi->fc;	break;
	}
    }
    return pi;
}

/**
 */
static int pkgNextIterator(/*@null@*/ void * this) {
    struct pkgIterator * pi = this;
    int i = -1;

    if (pi) {
	TFI_t fi = pi->fi;
	switch (fi->type) {
	case TR_ADDED:
	    if (pi->i < fi->fc)
		i = pi->i++;
	    break;
	case TR_REMOVED:
	    if (pi->i >= 0)
		i = --pi->i;
	    break;
	}
    }
    return i;
}

int pkgActions(const rpmTransactionSet ts, TFI_t fi, fileStage a)
{
    int rc = 0;

    if (fi->actions) {
	void * pi = pkgInitIterator(ts, fi);
	int i;
	while ((i = pkgNextIterator(pi)) != -1) {
	    if (pkgAction(ts, fi, i, a))
		rc++;
	}
	pi = pkgFreeIterator(pi);
    }
    return rc;
}
