#include "system.h"

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include "rpm/rpmlib.h"
#include "lib/rpmfs.h"
#include "rpm/rpmts.h"
#include "lib/rpmvs.h"
#include "lib/rpmdb_internal.h"
#include "lib/rpmchroot.h"
#include "rpm/rpmlog.h"
#include "rpm/rpmfileutil.h"

#include "debug.h"

#define ff(fmt, ...) rpmlog(RPMLOG_DEBUG, "%s() -- " fmt "\n", __FUNCTION__, __VA_ARGS__)

struct dbiCursor_s {
    unsigned i;
    const void *key;
    unsigned int keylen;
};

typedef struct filedb_s {
    int dirfd;
    unsigned nheaders;
    Header* headers;
    char** files;
} filedb_t;

static void load_headers(filedb_t* p)
{
    int dfd = dup(p->dirfd);
    DIR* d = fdopendir(dfd);
    struct dirent* entry;

    while ((entry = readdir(d))) {
        // strlen(1-3-5.rpm) == 9
        if (strlen(entry->d_name) < 9)
            continue;
        if (strcmp(entry->d_name+strlen(entry->d_name)-4, ".rpm"))
            continue;

        p->files = xreallocn(p->files, sizeof(char*), p->nheaders+1);
        p->headers = xreallocn(p->headers, sizeof(Header), p->nheaders+1);

        p->files[p->nheaders] = strdup(entry->d_name);

        FD_t fd = NULL;
        struct stat sb = {0};
        int hfd = openat(p->dirfd, entry->d_name, O_RDONLY);
        if (hfd >= 0) {
            fstat(hfd, &sb);
            fd = Fdopen(fdDup(hfd), "r.ufdio");
        } else {
                rpmlog(RPMLOG_ERR, "failed to open %s: %m\n", entry->d_name);
        }
        if (fd) {
            Header h;
            struct rpmvs_s *vs = rpmvsCreate(0, RPMVSF_MASK_NOPAYLOAD, NULL);
            rpmRC rc = rpmReadPackageHeader(vs, fd, NULL, &h);
            if (rc == RPMRC_OK || rc == RPMRC_NOKEY) {
                struct rpmtd_s basenames = {0};
                rpmfs fs = NULL;

                if (headerGet(h, RPMTAG_BASENAMES, &basenames, HEADERGET_MINMEM))
                    fs = rpmfsNew(rpmtdCount(&basenames), 1);

                headerAddInstallTags(h, sb.st_ctime, sb.st_ctime, 0, fs);

                rpmtdFreeData(&basenames);
                rpmfsFree(fs);

                p->headers[p->nheaders] = h;
            } else {
                rpmlog(RPMLOG_ERR, "failed to read %s %d\n", entry->d_name, rc);
            }
            Fclose(fd);
            rpmvsFree(vs);
        }
        ++p->nheaders;
    }

    closedir(d);
    close(dfd);
}

static int filedb_Close(dbiIndex dbi, unsigned int flags)
{
    rpmdb rdb = dbi->dbi_rpmdb;

    if (rdb->db_opens > 1) {
        rdb->db_opens--;
    } else {
        filedb_t* p =  rdb->db_dbenv;
        close(p->dirfd);
        for (int i = 0; i < p->nheaders; ++i) {
            free(p->files[i]);
            headerFree(p->headers[i]);
        }
        free(p->files);
        free(p->headers);
        free(rdb->db_dbenv);
        rdb->db_dbenv = NULL;
    }

    dbiFree(dbi);
    return 0;
}

static int filedb_Open(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
{
    if (rdb->db_dbenv == NULL) {
        char fn[PATH_MAX] = {0};
        char* c = fn;
        if (!rpmChrootDone())
            c = stpncpy(c, rdb->db_root, sizeof(fn)-(c-fn)-1);
        c = stpncpy(c, filedb_dbops.path, sizeof(fn)-(c-fn)-1);

        rpmioMkpath(fn, 0755, -1, -1);
        int fd = open(fn, O_CLOEXEC|O_DIRECTORY|O_NOCTTY|O_NOFOLLOW);
        if (fd < 0) {
            rpmlog(RPMLOG_ERR, "failed to open %s: %m\n", fn);
            return RPMRC_FAIL;
        }
        filedb_t* p =  xcalloc(1, sizeof(filedb_t));
        p->dirfd = fd;

        load_headers(p);

        rdb->db_dbenv = p;
    }
    rdb->db_opens++;

    if (dbip) {
        dbiIndex dbi = dbiNew(rdb, rpmtag);
        dbi->dbi_db = rdb->db_dbenv;
        *dbip = dbi;
    }
    return 0;
}

static int filedb_Verify(dbiIndex dbi, unsigned int flags)
{
    ff("%u", flags);
    return 0;
}

static void filedb_SetFSync(rpmdb rdb, int enable)
{
    ff("%d", enable);
}

static int filedb_Ctrl(rpmdb rdb, dbCtrlOp ctrl)
{
    ff("%d", ctrl);
    return 0;
}

static dbiCursor filedb_CursorInit(dbiIndex dbi, unsigned int flags)
{
    struct dbiCursor_s* dbc = rmalloc(sizeof(struct dbiCursor_s));
    memset(dbc, 0, sizeof(struct dbiCursor_s));
    return dbc;
}

static dbiCursor filedb_CursorFree(dbiIndex dbi, dbiCursor dbc)
{
    return rfree(dbc);
}

static rpmRC filedb_pkgdbPut(dbiIndex dbi, dbiCursor dbc,  unsigned int *hdrNum, unsigned char *hdrBlob, unsigned int hdrLen)
{
    rpmdb rdb = dbi->dbi_rpmdb;
    filedb_t* p =  rdb->db_dbenv;
    rpmRC rc = RPMRC_FAIL;
    char fn[PATH_MAX] = {0};
    /* this is crappy obviously. need differnet api.
       Copy is needed to not take ownership of the blob. */
    Header h = headerImport(hdrBlob, hdrLen, HEADERIMPORT_COPY);
    char *nevra = headerGetAsString(h, RPMTAG_NEVRA);

    snprintf(fn, sizeof(fn), "%s.rpm", nevra);
    nevra = rfree(nevra);

    FD_t fd = NULL;
    int hfd = openat(p->dirfd, fn, O_WRONLY|O_CREAT|O_EXCL, 0666);
    if (hfd < 0 && errno == EEXIST) {
        // As we don't store the hdrNum on disk, we need to move
        // files with same name way so pkgdbDel won't remove it.
        int i;
        for (i = 0; i < p->nheaders; ++i) {
            if (!strcmp(p->files[i], fn)) {
                char* newfn = NULL;
                rasprintf(&newfn, ".%sold", fn);
                renameat(p->dirfd, fn, p->dirfd, newfn);
                free(p->files[i]);
                p->files[i] = newfn;
                break;
            }
        }
        if (i >= p->nheaders) {
            rpmlog(RPMLOG_ERR, "%s exists but can't be found!?\n", fn);
            return RPMRC_FAIL;
        }
        hfd = openat(p->dirfd, fn, O_WRONLY|O_CREAT|O_EXCL, 0666);
    }
    if (hfd >= 0) {
        fd = Fdopen(fdDup(hfd), "w.ufdio");
    }
    if (!fd) {
            rpmlog(RPMLOG_ERR, "failed to open %s\n", fn);
            goto exit;
    }

    headerWriteAsPackage(fd, h);
    Fclose(fd);

    p->files = xreallocn(p->files, sizeof(char*), p->nheaders+1);
    p->headers = xreallocn(p->headers, sizeof(Header), p->nheaders+1);
    p->files[p->nheaders] = strdup(fn);
    p->headers[p->nheaders] = h;
    *hdrNum = ++p->nheaders;

    rc = RPMRC_OK;

exit:
    if (rc != RPMRC_OK)
        headerFree(h);
    return rc;
}

static rpmRC filedb_pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum)
{
    rpmdb rdb = dbi->dbi_rpmdb;
    filedb_t* p =  rdb->db_dbenv;

    if (hdrNum > p->nheaders)
        return RPMRC_FAIL;

    int i = hdrNum-1;
    if (!p->headers[i])
        return RPMRC_NOTFOUND;

    if(unlinkat(p->dirfd, p->files[i], 0)) {
        rpmlog(RPMLOG_ERR, "failed to unlink %s: %m\n", p->files[i]);
        return RPMRC_FAIL;
    }
    p->files[i] = rfree(p->files[i]);
    p->headers[i] = rfree(p->headers[i]);
    return RPMRC_OK;
}

static rpmRC filedb_pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char **hdrBlob, unsigned int *hdrLen)
{
    rpmdb rdb = dbi->dbi_rpmdb;
    filedb_t* p =  rdb->db_dbenv;
    if (hdrNum) {
        if (hdrNum <= p->nheaders) {
            *hdrBlob = headerExport(p->headers[hdrNum-1], hdrLen);
            return RPMRC_OK;
        }

        return RPMRC_NOTFOUND;
    } else if (dbc->i < p->nheaders) {
        *hdrBlob = headerExport(p->headers[dbc->i], hdrLen);
        ++dbc->i;
        return RPMRC_OK;
    }

    return RPMRC_NOTFOUND;
}

static unsigned int filedb_pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    return dbc->i;
}

static rpmRC filedb_idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int searchType)
{
    // that's weird. would make sense for dbi to contain the enum?
    rpmTagVal tag = rpmTagGetValue(dbi->dbi_file);
    rpmdb rdb = dbi->dbi_rpmdb;
    filedb_t* p =  rdb->db_dbenv;
    if (rpmTagGetTagType(tag) == RPM_STRING_TYPE) {
        int found = 0;
        rpmlog(RPMLOG_DEBUG, "looking for %s (%s)\n", keyp, searchType == DBC_PREFIX_SEARCH?"substr":"exact");
        for (int i = 0; i < p->nheaders; ++i) {
            Header h = p->headers[i];
            if (!h) // deleted
                continue;
            const char *val = headerGetString(h, tag);
            if (!val) {
                rpmlog(RPMLOG_ERR, "failed to query %s at header %u\n", rpmTagGetName(tag), i);
                continue;
            }

            if (keyp) {
                if (strncmp(val, keyp, keylen))
                    continue;
                if (searchType == DBC_NORMAL_SEARCH && strlen(val) != keylen)
                    continue;
            }
            found = 1;
            if (*set == NULL)
                *set = dbiIndexSetNew(0);
            dbc->key = val;
            dbc->keylen = strlen(val);
            dbiIndexSetAppendOne(*set, i+1, 0, 0);
        }
        if (found)
            return RPMRC_OK;
    }
    return RPMRC_NOTFOUND;
}

static rpmRC filedb_idxdbPut(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h)
{
    return RPMRC_OK;
}

static rpmRC filedb_idxdbDel(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h) {
    return RPMRC_OK;
}

static const void * filedb_idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    const void *key = NULL;
    if (dbc) {
        key = dbc->key;
        if (key && keylen)
            *keylen = dbc->keylen;
    }
    return key;
}

struct rpmdbOps_s filedb_dbops = {
    .name	= "file",
    .path	= "/usr/lib/sysimage/rpm-headers",

    .open	= filedb_Open,
    .close	= filedb_Close,
    .verify	= filedb_Verify,
    .setFSync	= filedb_SetFSync,
    .ctrl	= filedb_Ctrl,

    .cursorInit	= filedb_CursorInit,
    .cursorFree	= filedb_CursorFree,

    .pkgdbPut	= filedb_pkgdbPut,
    .pkgdbDel	= filedb_pkgdbDel,
    .pkgdbGet	= filedb_pkgdbGet,
    .pkgdbKey	= filedb_pkgdbKey,

    .idxdbGet	= filedb_idxdbGet,
    .idxdbPut	= filedb_idxdbPut,
    .idxdbDel	= filedb_idxdbDel,
    .idxdbKey	= filedb_idxdbKey
};

// vim: sw=4 et
