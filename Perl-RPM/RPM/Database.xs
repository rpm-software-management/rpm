#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <fcntl.h>
#include "RPM.h"

static char * const rcsid = "$Id: Database.xs,v 1.16 2002/04/11 22:41:15 rjray Exp $";

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
        retvalp->noffs = retvalp->offx = 0;
        retvalp->offsets = (int *)NULL;
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
    rpmdbMatchIterator mi;
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
            return newSVsv(*svp);

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

    mi = rpmdbInitIterator(dbstruct->dbp, RPMDBI_PACKAGES,
                           &offset, sizeof(offset));
    hdr = rpmdbNextIterator(mi);

    if (hdr)
    {
        hdr = headerLink(hdr);
        FETCHp = rpmhdr_TIEHASH(aTHX_ "RPM::Header",
                                sv_2mortal(newSViv((unsigned)hdr)),
                                RPM_HEADER_FROM_REF | RPM_HEADER_READONLY);
        if (name == Null(const char *))
            name = SvPV(rpmhdr_FETCH(aTHX_ FETCHp,
                                     sv_2mortal(newSVpv("NAME", 4)),
                                     Null(const char *), 0, 0), namelen);
        FETCH = sv_bless(newRV_noinc((SV*)FETCHp),
                         gv_stashpv("RPM::Header", TRUE));
        hv_store(dbstruct->storage, (char *)name, namelen, newSVsv(FETCH),
                 FALSE);
    }
    rpmdbFreeIterator(mi);

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

    if (dbstruct->offsets == NULL || dbstruct->noffs <= 0)
         return 0;
    if (dbstruct->offx >= dbstruct->noffs)
        return 0;

    dbstruct->current_rec = dbstruct->offsets[dbstruct->offx++];

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
    if (dbstruct->offsets)
        safefree(dbstruct->offsets);

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
    rpmdbMatchIterator mi;

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
