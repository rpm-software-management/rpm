/** \ingroup rpmtrans payload
 * \file lib/rollback.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rollback.h"

#include "debug.h"

/*@access h@*/		/* compared with NULL */

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

static char * ridsub = ".rid/";
static char * ridsep = ";";
static mode_t riddmode = 0700;
static mode_t ridfmode = 0000;

int dirstashPackage(const rpmTransactionSet ts, const TFI_t fi, rollbackDir dir)
{
    unsigned int offset = fi->record;
    char tsid[20], ofn[BUFSIZ], nfn[BUFSIZ];
    const char * s, * t;
    char * se, * te;
    Header h;
    int i;

    assert(fi->type == TR_REMOVED);

    {	rpmdbMatchIterator mi = NULL;

	mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES,
				&offset, sizeof(offset));

	h = rpmdbNextIterator(mi);
	if (h == NULL) {
	    rpmdbFreeIterator(mi);
	    return 1;
	}
	h = headerLink(h);
	rpmdbFreeIterator(mi);
    }

    sprintf(tsid, "%08x", ts->id);

    /* Create rid sub-directories if necessary. */
    if (strchr(ridsub, '/')) {
	for (i = 0; i < fi->dc; i++) {

	    t = te = nfn;
	    *te = '\0';
	    te = stpcpy(te, fi->dnl[i]);
	    te = stpcpy(te, ridsub);
	    if (te[-1] == '/')
		*(--te) = '\0';
fprintf(stderr, "*** mkdir(%s,%o)\n", t, (int)riddmode);
	}
    }

    /* Rename files about to be removed. */
    for (i = 0; i < fi->fc; i++) {

	if (S_ISDIR(fi->fmodes[i]))
	    continue;

	s = se = ofn;
	*se = '\0';
	se = stpcpy( stpcpy(se, fi->dnl[fi->dil[i]]), fi->bnl[i]);

	t = te = nfn;
	*te = '\0';
	te = stpcpy(te, fi->dnl[fi->dil[i]]);
	if (ridsub)
	    te = stpcpy(te, ridsub);
	te = stpcpy( stpcpy( stpcpy(te, fi->bnl[i]), ridsep), tsid);

	s = strrchr(s, '/') + 1;
	t = strrchr(t, '/') + 1;
fprintf(stderr, "*** rename(%s,%s%s)\n", s, (ridsub ? ridsub : ""), t);
fprintf(stderr, "*** chmod(%s%s,%o)\n", (ridsub ? ridsub : ""), t, ridfmode);
    }

    return 0;
}

void loadFi(Header h, TFI_t fi)
{
    HGE_t hge;
    int len, i;
    
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

    if (!hge(fi->h, RPMTAG_BASENAMES, NULL, (void **) &fi->bnl, &fi->fc)) {
	fi->dc = 0;
	fi->fc = 0;
	fi->dnl = NULL;
	fi->bnl = NULL;
	fi->dil = NULL;
	fi->fmodes = NULL;
	fi->fflags = NULL;
	fi->fsizes = NULL;
	fi->fstates = NULL;
	fi->fmd5s = NULL;
	fi->flinks = NULL;
	fi->flangs = NULL;
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
    if (fi->h) {
	headerFree(fi->h); fi->h = NULL;
    }
    if (fi->name) {
	free((void *)fi->name); fi->name = NULL;
    }
    if (fi->version) {
	free((void *)fi->version); fi->version = NULL;
    }
    if (fi->release) {
	free((void *)fi->release); fi->release = NULL;
    }
    if (fi->actions) {
	free(fi->actions); fi->actions = NULL;
    }
    if (fi->replacedSizes) {
	free(fi->replacedSizes); fi->replacedSizes = NULL;
    }
    if (fi->replaced) {
	free(fi->replaced); fi->replaced = NULL;
    }
    if (fi->bnl) {
	free(fi->bnl); fi->bnl = NULL;
    }
    if (fi->dnl) {
	free(fi->dnl); fi->dnl = NULL;
    }
    if (fi->obnl) {
	free(fi->obnl); fi->obnl = NULL;
    }
    if (fi->odnl) {
	free(fi->odnl); fi->odnl = NULL;
    }
    if (fi->flinks) {
	free(fi->flinks); fi->flinks = NULL;
    }
    if (fi->fmd5s) {
	free(fi->fmd5s); fi->fmd5s = NULL;
    }
    if (fi->fuser) {
	free(fi->fuser); fi->fuser = NULL;
    }
    if (fi->fgroup) {
	free(fi->fgroup); fi->fgroup = NULL;
    }
    if (fi->flangs) {
	free(fi->flangs); fi->flangs = NULL;
    }
    if (fi->apath) {
	free(fi->apath); fi->apath = NULL;
    }
    if (fi->fuids) {
	free(fi->fuids); fi->fuids = NULL;
    }
    if (fi->fgids) {
	free(fi->fgids); fi->fgids = NULL;
    }
    if (fi->fmapflags) {
	free(fi->fmapflags); fi->fmapflags = NULL;
    }

    switch (fi->type) {
    case TR_ADDED:
	    break;
    case TR_REMOVED:
	if (fi->fsizes) {
	    free((void *)fi->fsizes); fi->fsizes = NULL;
	}
	if (fi->fflags) {
	    free((void *)fi->fflags); fi->fflags = NULL;
	}
	if (fi->fmodes) {
	    free((void *)fi->fmodes); fi->fmodes = NULL;
	}
	if (fi->fstates) {
	    free((void *)fi->fstates); fi->fstates = NULL;
	}
	if (fi->dil) {
	    free((void *)fi->dil); fi->dil = NULL;
	}
	break;
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
    case FI_CREATE:	return "create";
    case FI_INIT:	return "init";
    case FI_MAP:	return "map";
    case FI_SKIP:	return "skip";
    case FI_PRE:	return "pre-process";
    case FI_PROCESS:	return "process";
    case FI_POST:	return "post-process";
    case FI_NOTIFY:	return "notify";
    case FI_UNDO:	return "undo";
    case FI_COMMIT:	return "commit";
    case FI_DESTROY:	return "destroy";
    default:		return "???";
    }
    /*@noteached@*/
}

/*@obserever@*/ const char *const fileActionString(fileAction a)
{
    switch (a) {
    case FA_UNKNOWN: return "unknown";
    case FA_CREATE: return "create";
    case FA_BACKUP: return "backup";
    case FA_SAVE: return "save";
    case FA_SKIP: return "skip";
    case FA_ALTNAME: return "altname";
    case FA_REMOVE: return "remove";
    case FA_SKIPNSTATE: return "skipnstate";
    case FA_SKIPNETSHARED: return "skipnetshared";
    case FA_SKIPMULTILIB: return "skipmultilib";
    default: return "???";
    }
    /*@notreached@*/
}

struct pkgIterator {
/*@kept@*/ TFI_t fi;
    int i;
};

static void pkgFreeIterator(void * this) {
    if (this) free(this);
}

static void * pkgInitIterator(TFI_t fi) {
    struct pkgIterator *pi = xcalloc(sizeof(*pi), 1);
    pi->fi = fi;
    switch (fi->type) {
    case TR_ADDED:	pi->i = 0;		break;
    case TR_REMOVED:	pi->i = fi->fc;	break;
    }
    return pi;
}

static int pkgNextIterator(void * this) {
    struct pkgIterator *pi = this;
    TFI_t fi = pi->fi;
    int i = -1;
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
    return i;
}


int pkgActions(const rpmTransactionSet ts, TFI_t fi)
{
    int nb = (!ts->chrootDone ? strlen(ts->rootDir) : 0);
    char * opath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * o = (!ts->chrootDone ? stpcpy(opath, ts->rootDir) : opath);
    char * npath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * n = (!ts->chrootDone ? stpcpy(npath, ts->rootDir) : npath);
    int rc = 0;
    void * pi;
    int i;

    if (fi->actions == NULL)
	return rc;

    pi = pkgInitIterator(fi);
    while ((i = pkgNextIterator(pi)) != -1) {
	char * ext, * t;

	if (fi->actions[i] & FA_DONE)
	    continue;

	rpmMessage(RPMMESS_DEBUG, _("   file: %s%s action: %s\n"),
			fi->dnl[fi->dil[i]], fi->bnl[i],
		fileActionString((fi->actions ? fi->actions[i] : FA_UNKNOWN)) );

	ext = NULL;

	switch (fi->actions[i] & ~FA_DONE) {
	case FA_DONE:
	case FA_SKIP:
	case FA_SKIPMULTILIB:
	case FA_UNKNOWN:
	    continue;
	    /*@notreached@*/ break;

	case FA_CREATE:
	    assert(fi->type == TR_ADDED);
	    continue;
	    /*@notreached@*/ break;

	case FA_SKIPNSTATE:
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NOTINSTALLED;
	    continue;
	    /*@notreached@*/ break;

	case FA_SKIPNETSHARED:
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NETSHARED;
	    continue;
	    /*@notreached@*/ break;
	case FA_BACKUP:
	    ext = (fi->type == TR_ADDED ? SUFFIX_RPMORIG : SUFFIX_RPMSAVE);
	    break;

	case FA_ALTNAME:
	    assert(fi->type == TR_ADDED);
	    ext = SUFFIX_RPMNEW;
	    t = xmalloc(strlen(fi->bnl[i]) + strlen(ext) + 1);
	    (void)stpcpy(stpcpy(t, fi->bnl[i]), ext);
	    rpmMessage(RPMMESS_WARNING, _("%s: %s%s created as %s\n"),
			fiTypeString(fi), fi->dnl[fi->dil[i]], fi->bnl[i], t);
	    fi->bnl[i] = t;		/* XXX memory leak iff i = 0 */
	    ext = NULL;
	    continue;
	    /*@notreached@*/ break;

	case FA_SAVE:
	    assert(fi->type == TR_ADDED);
	    ext = SUFFIX_RPMSAVE;
	    break;

	case FA_REMOVE:
	    assert(fi->type == TR_REMOVED);
	    /* Append file name to (possible) root dir. */
	    (void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);
	    if (S_ISDIR(fi->fmodes[i])) {
		if (!rmdir(opath))
		    continue;
		switch (errno) {
		case ENOENT: /* XXX rmdir("/") linux 2.2.x kernel hack */
		case ENOTEMPTY:
#ifdef	NOTYET
		    if (fi->fflags[i] & RPMFILE_MISSINGOK)
			continue;
#endif
		    rpmError(RPMERR_RMDIR, 
			_("%s: cannot remove %s - directory not empty\n"), 
				fiTypeString(fi), o);
		    break;
		default:
		    rpmError(RPMERR_RMDIR,
				_("%s rmdir of %s failed: %s\n"),
				fiTypeString(fi), o, strerror(errno));
		    break;
		}
		rc++;
		continue;
		/*@notreached@*/ break;
	    }
	    if (!unlink(opath))
		continue;
	    if (errno == ENOENT && (fi->fflags[i] & RPMFILE_MISSINGOK))
		continue;
	    rpmError(RPMERR_UNLINK, _("%s removal of %s failed: %s\n"),
				fiTypeString(fi), o, strerror(errno));
	    rc++;
	    continue;
	    /*@notreached@*/ break;
	}

	if (ext == NULL)
	    continue;

	/* Append file name to (possible) root dir. */
	(void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);

	/* XXX TR_REMOVED dinna do this. */
	if (access(opath, F_OK) != 0)
	    continue;

	(void) stpcpy( stpcpy(n, o), ext);
	rpmMessage(RPMMESS_WARNING, _("%s: %s saved as %s\n"),
			fiTypeString(fi), o, n);

	if (!rename(opath, npath))
	    continue;

	rpmError(RPMERR_RENAME, _("%s rename of %s to %s failed: %s\n"),
			fiTypeString(fi), o, n, strerror(errno));
	rc++;
    }
    pkgFreeIterator(pi);
    return rc;
}
