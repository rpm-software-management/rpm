#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <fcntl.h>
#include "RPM.h"

static char * const rcsid = "$Id: Database.xs,v 1.13 2001/03/08 06:11:20 rjray Exp $";

/*
  rpmdb_TIEHASH

  This is the implementation of the tied-hash class constructor. The XS
  wrapper will verify that the value of class is correct, then massage the
  arguments as needed. The return value is expected to be either NULL or a
  valid RPM__Database value (which the XS wrapper will fix up).
*/
RPM__Database rpmdb_TIEHASH(pTHX_ char* class, SV* opts)
{
    char*  root  = (char *)NULL;
    int    mode  = O_RDONLY;
    mode_t perms = 0;
    HV*    opt_hash;
    SV*    t_magic;
    SV**   svp;
    RPM_Database* retvalp; /* For "private" */
    RPM__Database RETVAL;

    if (opts)
    {
        if (SvROK(opts) && (SvTYPE(opts) == SVt_PVHV))
        {
            /* This is a hash reference. We are concerned only with
               the key "root". "mode" and "perms" don't apply, as we are
               going to open the database as read-only. */
            opt_hash = (HV*)SvRV(opts);

            svp = hv_fetch(opt_hash, "root", 4, FALSE);
            if (svp && SvPOK(*svp))
                root = SvPV(*svp, PL_na);
        }
        else if (SvPOK(opts))
        {
            /* They passed a scalar, assumed to be the "root" */
            root = SvPV(opts, PL_na);
        }
        else
        {
            rpm_error(aTHX_ RPMERR_BADARG,
                      "Wrong type for argument 2 to TIEHASH");
            return (Null(RPM__Database));
        }
    }

    /* With that all processed, attempt to open the actual RPM DB */
    /* The retvalp is used for the C-level rpmlib information on databases */
    retvalp = new_RPM_storage(RPM_Database);
    if (rpmdbOpen(root, &retvalp->dbp, mode, perms) != 0)
        /* rpm lib will have set the error already */
        return (Null(RPM__Database));
    else
    {
        retvalp->current_rec = 0;
#if RPM_MAJOR < 4
        retvalp->index_set = (void *)NULL;
#else
        retvalp->noffs = retvalp->offx = 0;
        retvalp->offsets = (int *)NULL;
#endif
    }

    RETVAL = newHV();
    retvalp->storage = newHV();
    t_magic = newSViv((unsigned)retvalp);

    sv_magic((SV *)RETVAL, Nullsv, 'P', Nullch, 0);
    sv_magic((SV *)RETVAL, t_magic, '~', Nullch, 0);
    SvREFCNT_dec(t_magic);

    return RETVAL;
}

SV* rpmdb_FETCH(pTHX_ RPM__Database self, SV* key)
{
    const char* name = NULL; /* For the actual name out of (SV *)key */
    int namelen;             /* Arg for SvPV(..., len)               */
    int offset;              /* In case they pass an integer offset  */
    Header hdr;              /* For rpmdbGetRecord() calls           */
#if RPM_MAJOR >= 4
    rpmdbMatchIterator mi;
#else
    int result;
#endif
    SV** svp;
    SV* FETCH;
    RPM__Header FETCHp;
    RPM_Database* dbstruct;  /* This is the struct used to hold C-level data */

    struct_from_object_ret(RPM_Database, dbstruct, self, FETCH);
    /* De-reference key, if it is a reference */
    if (SvROK(key))
        key = SvRV(key);

    /* For sake of flexibility (and because it's almost zero overhead),
       allow the request to be by name -or- by an offset number */
    if (SvPOK(key))
    {
        name = SvPV(key, namelen);

        /* Step 1: Check to see if this has already been requested and is
           thus cached on the hash itself */
        svp = hv_fetch(dbstruct->storage, (char *)name, namelen, FALSE);
        if (svp && SvROK(*svp))
            return newRV(SvRV(*svp));

#if RPM_MAJOR < 4
        /* This is the old (3.0.4+) way of setting up and searching */
        /* Create an index set if we don't already have one */
        if (! dbstruct->index_set)
        {
            dbstruct->index_set =
                (dbiIndexSet *)safemalloc(sizeof(dbiIndexSet));
            Zero(dbstruct->index_set, 1, dbiIndexSet);
        }
        /* Run the search */
        result = rpmdbFindPackage(dbstruct->dbp, name, dbstruct->index_set);
        if (result)
            /* Some sort of error occured when reading the DB or the
               name was not found. */
            return &PL_sv_undef;
        else
        {
            /* There may have been more than one match, but for now
               I can only take the first one off the list. */
            if (dbstruct->index_set->count)
            {
                offset = dbstruct->index_set->recs[0].recOffset;
            }
            else
            {
                /* In theory, this wouldn't happen since zero matches
                   would mean a return value of 1 from the library.
                   But I ain't betting the core on that... */
                return &PL_sv_undef;
            }
        }
#else
        /* This is the 4.0 way */
        offset = -1;
        mi =  rpmdbInitIterator(dbstruct->dbp, RPMTAG_NAME, name, 0);
        while ((hdr = rpmdbNextIterator(mi)) != NULL)
        {
            offset = rpmdbGetIteratorOffset(mi);
            break;
        }
        rpmdbFreeIterator(mi);
        if (offset == -1)
            /* Some sort of error occured when reading the DB or the
               name was not found. */
            return &PL_sv_undef;
#endif
    }
    else if (SvIOK(key))
    {
        /* This is actually a lot easier than fetch-by-name, which is
           why I've thrown it in */
        offset = SvIV(key);
    }
    else
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Database::FETCH: Second arg should be name or offset");
        return &PL_sv_undef;
    }

#if RPM_MAJOR < 4
    hdr = rpmdbGetRecord(dbstruct->dbp, offset);
    /* An error results in hdr getting NULL, which is just fine */
#else
    mi = rpmdbInitIterator(dbstruct->dbp, RPMDBI_PACKAGES,
                           &offset, sizeof(offset));
    hdr = rpmdbNextIterator(mi);
#endif
    if (hdr)
    {
#if RPM_MAJOR >= 4
        hdr = headerLink(hdr);
#endif
        FETCHp = rpmhdr_TIEHASH(aTHX_ "RPM::Header",
                                sv_2mortal(newSViv((unsigned)hdr)),
                                RPM_HEADER_FROM_REF | RPM_HEADER_READONLY);
        FETCH = sv_bless(newRV_noinc((SV*)FETCHp),
                         gv_stashpv("RPM::Header", TRUE));
        /* If name is no longer NULL, it means our vector in was a string
           (key), so put the result back into the hash-cache. */
        if (name != NULL)
            hv_store(dbstruct->storage, (char *)name, namelen, FETCH, FALSE);
    }
#if RPM_MAJOR >= 4
    rpmdbFreeIterator(mi);
#endif

    return FETCH;
}

bool rpmdb_EXISTS(pTHX_ RPM__Database self, SV* key)
{
    SV* tmp;

    tmp = rpmdb_FETCH(aTHX_ self, key);
    return (tmp != &PL_sv_undef);
}

/*
  This is quite a bit easier than the FIRSTKEY/NEXTKEY combo for headers.
  In these cases, the transition is based on the last offset fetched, which
  we store on the struct part of self. We don't have to worry about an
  iterator struct.
*/
int rpmdb_FIRSTKEY(pTHX_ RPM__Database self, SV** key, SV** value)
{
    RPM_Database* dbstruct;

    struct_from_object_ret(RPM_Database, dbstruct, self, 0);
#if RPM_MAJOR < 4
    /* This more or less resets our "iterator" */
    dbstruct->current_rec = 0;

    if (! (dbstruct->current_rec = rpmdbFirstRecNum(dbstruct->dbp)))
        return 0;
#else
    if (dbstruct->offsets == NULL || dbstruct->noffs <= 0)
    {
        rpmdbMatchIterator mi;
        Header h;

        if (dbstruct->offsets)
            free(dbstruct->offsets);
        dbstruct->offsets = NULL;
        dbstruct->noffs = 0;
        mi = rpmdbInitIterator(dbstruct->dbp, RPMDBI_PACKAGES, NULL, 0);
        while ((h = rpmdbNextIterator(mi)) != NULL)
        {
            dbstruct->noffs++;
            dbstruct->offsets =
                realloc(dbstruct->offsets,
                        dbstruct->noffs * sizeof(dbstruct->offsets[0]));
            dbstruct->offsets[dbstruct->noffs-1] = rpmdbGetIteratorOffset(mi);
        }
        rpmdbFreeIterator(mi);
    }

    if (dbstruct->offsets == NULL || dbstruct->noffs <= 0)
        return 0;

    dbstruct->offx = 0;
    dbstruct->current_rec = dbstruct->offsets[dbstruct->offx++];
#endif

    *value = rpmdb_FETCH(aTHX_ self, newSViv(dbstruct->current_rec));
    *key = rpmhdr_FETCH(aTHX_ (RPM__Header)SvRV(*value), newSVpv("name", 4),
                        Nullch, 0, 0);

    return 1;
}

int rpmdb_NEXTKEY(pTHX_ RPM__Database self, SV* key,
                  SV** nextkey, SV** nextvalue)
{
    RPM_Database* dbstruct;

    struct_from_object_ret(RPM_Database, dbstruct, self, 0);

#if RPM_MAJOR < 4
    if (! (dbstruct->current_rec = rpmdbNextRecNum(dbstruct->dbp,
                                                   dbstruct->current_rec)))
        return 0;
#else
    if (dbstruct->offsets == NULL || dbstruct->noffs <= 0)
         return 0;
    if (dbstruct->offx >= dbstruct->noffs)
        return 0;

    dbstruct->current_rec = dbstruct->offsets[dbstruct->offx++];
#endif

    *nextvalue = rpmdb_FETCH(aTHX_ self, newSViv(dbstruct->current_rec));
    *nextkey = rpmhdr_FETCH(aTHX_ (RPM__Header)SvRV(*nextvalue),
                            newSVpv("name", 4), Nullch, 0, 0);

    return 1;
}

void rpmdb_DESTROY(pTHX_ RPM__Database self)
{
    RPM_Database* dbstruct;  /* This is the struct used to hold C-level data */

    struct_from_object(RPM_Database, dbstruct, self);

    rpmdbClose(dbstruct->dbp);
#if RPM_MAJOR < 4
    if (dbstruct->index_set)
        dbiFreeIndexRecord(*dbstruct->index_set);
#else
    if (dbstruct->offsets)
        safefree(dbstruct->offsets);
#endif

    hv_undef(dbstruct->storage);
    safefree(dbstruct);
    hv_undef(self);
}

int rpmdb_init(SV* class, const char* root, int perms)
{
    return (1 - rpmdbInit(root, perms));
}

int rpmdb_rebuild(SV* class, const char* root)
{
    return (1 - rpmdbRebuild(root));
}

/*
  This is a front-end to all the rpmdbFindBy*() set, including FindByPackage
  which differs from FETCH above in that if there is actually more than one
  match, all will be returned.
*/
AV* rpmdb_find_by_whatever(pTHX_ RPM__Database self, SV* string, int idx)
{
    const char* str = NULL; /* For the actual string out of (SV *)string    */
    RPM_Database* dbstruct; /* This is the struct used to hold C-level data */
    AV* return_val;
    int loop;
    SV* tmp_hdr;
#if RPM_MAJOR >= 4
    rpmdbMatchIterator mi;
#else
    int result;
#endif

    /* Any successful operation will store items on this */
    return_val = newAV();

    struct_from_object_ret(RPM_Database, dbstruct, self, return_val);
    if (sv_derived_from(string, "RPM::Header"))
    {
        SV* fetch_string = rpmhdr_FETCH(aTHX_ (RPM__Header)SvRV(string),
                                        sv_2mortal(newSVpvn("NAME", 4)),
                                        Nullch, 0, 0);

        str = SvPV(fetch_string, PL_na);
    }
    else
    {
        /* De-reference key, if it is a reference */
        if (SvROK(string))
            string = SvRV(string);
        /* Get the string */
        str = SvPV(string, PL_na);
    }

#if RPM_MAJOR < 4
    /* Create an index set if we don't already have one */
    if (! dbstruct->index_set)
    {
        dbstruct->index_set = (dbiIndexSet *)safemalloc(sizeof(dbiIndexSet));
        Zero(dbstruct->index_set, 1, dbiIndexSet);
    }
    if (idx == RPMTAG_BASENAMES)
        result = rpmdbFindByFile(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == RPMTAG_GROUP)
        result = rpmdbFindByGroup(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == RPMTAG_PROVIDENAME)
        result = rpmdbFindByProvides(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == RPMTAG_REQUIRENAME)
        result = rpmdbFindByRequiredBy(dbstruct->dbp,str, dbstruct->index_set);
    else if (idx == RPMTAG_CONFLICTNAME)
        result = rpmdbFindByConflicts(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == RPMTAG_NAME)
        result = rpmdbFindPackage(dbstruct->dbp, str, dbstruct->index_set);

    /* The various rpmdbFind*() routines return 0 on success */
    if (! result)
    {
        av_extend(return_val, dbstruct->index_set->count);
        for (loop = 0; loop < dbstruct->index_set->count; loop++)
        {
            idx = dbstruct->index_set->recs[loop].recOffset;
            tmp_hdr = rpmdb_FETCH(aTHX_ self, sv_2mortal(newSViv(idx)));
            av_store(return_val, loop, sv_2mortal(newSVsv(tmp_hdr)));
        }
    }
#else
    mi = rpmdbInitIterator(dbstruct->dbp, idx, str, 0);
    if (mi)
    {
        av_extend(return_val, rpmdbGetIteratorCount(mi));
        loop = 0;
        while ((rpmdbNextIterator(mi)) != NULL)
        {
            idx = rpmdbGetIteratorOffset(mi);
            tmp_hdr = rpmdb_FETCH(aTHX_ self, sv_2mortal(newSViv(idx)));
            av_store(return_val, loop++, sv_2mortal(newSVsv(tmp_hdr)));
        }
    }
    rpmdbFreeIterator(mi);
#endif

    return return_val;
}


MODULE = RPM::Database  PACKAGE = RPM::Database         PREFIX = rpmdb_


RPM::Database
rpmdb_TIEHASH(class, opts=NULL)
    char* class;
    SV* opts;
    PROTOTYPE: $;$
    CODE:
    RETVAL = rpmdb_TIEHASH(aTHX_ class, opts);
    OUTPUT:
    RETVAL

SV*
rpmdb_FETCH(self, key)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmdb_FETCH(aTHX_ self, key);
    OUTPUT:
    RETVAL

int
rpmdb_STORE(self=NULL, key=NULL, value=NULL)
    SV* self;
    SV* key;
    SV* value;
    PROTOTYPE: $$$
    CODE:
    {
        rpm_error(aTHX_ RPMERR_NOCREATEDB, "STORE: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
        RETVAL

SV*
rpmdb_DELETE(self=NULL, key=NULL)
    SV* self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    {
        rpm_error(aTHX_ RPMERR_NOCREATEDB, "DELETE: operation not permitted");
        RETVAL = Nullsv;
    }
    OUTPUT:
    RETVAL

int
rpmdb_CLEAR(self=NULL)
    SV* self;
    PROTOTYPE: $
    CODE:
    {
        rpm_error(aTHX_ RPMERR_NOCREATEDB, "CLEAR: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
    RETVAL

bool
rpmdb_EXISTS(self, key)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmdb_EXISTS(aTHX_ self, key);
    OUTPUT:
    RETVAL

void
rpmdb_FIRSTKEY(self)
    RPM::Database self;
    PROTOTYPE: $
    PPCODE:
    {
        SV* key;
        SV* value;

        if (! rpmdb_FIRSTKEY(aTHX_ self, &key, &value))
        {
            key = newSVsv(&PL_sv_undef);
            value = newSVsv(&PL_sv_undef);
        }

        EXTEND(SP, 2);
        PUSHs(sv_2mortal(value));
        PUSHs(sv_2mortal(newSVsv(key)));
    }

void
rpmdb_NEXTKEY(self, key=NULL)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $;$
    PPCODE:
    {
        SV* nextkey;
        SV* nextvalue;

        if (! rpmdb_NEXTKEY(aTHX_ self, key, &nextkey, &nextvalue))
        {
            nextkey = newSVsv(&PL_sv_undef);
            nextvalue = newSVsv(&PL_sv_undef);
        }

        EXTEND(SP, 2);
        PUSHs(sv_2mortal(nextvalue));
        PUSHs(sv_2mortal(newSVsv(nextkey)));
    }

void
rpmdb_DESTROY(self)
    RPM::Database self;
    PROTOTYPE: $
    CODE:
    rpmdb_DESTROY(aTHX_ self);

int
rpmdb_init(class, root=NULL, perms=O_RDWR)
    SV* class;
    const char* root;
    int perms;
    PROTOTYPE: $;$$
    INIT:
    if (! (SvPOK(class) && strcmp(SvPV(class, PL_na), "RPM::Database")))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Database::init must be called as a static method");
        ST(0) = sv_2mortal(newSViv(0));
        XSRETURN(1);
    }

int
rpmdb_rebuild(class, root=NULL)
    SV* class;
    const char* root;
    PROTOTYPE: $;$
    INIT:
    if (! (SvPOK(class) && strcmp(SvPV(class, PL_na), "RPM::Database")))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Database::rebuild must be called as a static method");
        ST(0) = sv_2mortal(newSViv(0));
        XSRETURN(1);
    }

void
rpmdb_find_by_file(self, string)
    RPM::Database self;
    SV* string;
    PROTOTYPE: $$
    ALIAS:
        find_by_group = RPMTAG_GROUP
        find_what_provides = RPMTAG_PROVIDENAME
        find_what_requires = RPMTAG_REQUIRENAME
        find_what_conflicts = RPMTAG_CONFLICTNAME
        find_by_package = RPMTAG_NAME
    PPCODE:
    {
        AV* matches;
        int len, size;

        if (ix == 0) ix = RPMTAG_BASENAMES;
        matches = rpmdb_find_by_whatever(aTHX_ self, string, ix);
        if ((len = av_len(matches)) != -1)
        {
            /* We have (len+1) elements in the array to put onto the stack */
            size = len + 1;
            EXTEND(SP, size);
            while (len >= 0)
            {
                /* This being a stack and all, we put them in backwards */
                PUSHs(sv_2mortal(newSVsv(*av_fetch(matches, len, FALSE))));
                len--;
            }
        }
        else
            size = 0;

        XSRETURN(size);
    }
