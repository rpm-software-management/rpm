#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <fcntl.h>
#include "RPM.h"

static char * const rcsid = "$Id: Database.xs,v 1.2 2000/05/30 01:03:13 rjray Exp $";

//
// Use this define for deriving the saved rpmdb struct, rather than coding
// it a dozen places. Note that the hv_fetch call is the no-magic one defined
// in RPM.h
//
#define dbstruct_from_object_ret(s_ptr, rdb, object, err_ret) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (rdb) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Database *)SvIV(*(s_ptr)) : NULL; \
    if (! (rdb)) return (err_ret);
// And a no-return-value version:
#define dbstruct_from_object(s_ptr, rdb, object) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (rdb) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Database *)SvIV(*(s_ptr)) : NULL; \
    if (! (rdb)) return;

//
// rpmdb_TIEHASH
//
// This is the implementation of the tied-hash class constructor. The XS
// wrapper will verify that the value of class is correct, then massage the
// arguments as needed. The return value is expected to be either NULL or a
// valid RPM__Database value (which the XS wrapper will fix up).
//
RPM__Database rpmdb_TIEHASH(pTHX_ char* class, SV* opts)
{
    char*  root  = (char *)NULL;
    int    mode  = O_RDONLY;
    mode_t perms = 0;
    HV*    opt_hash;
    SV*    value;
    SV**   svp;
    int    root_len;
    RPM_Database* dbstruct;
    RPM__Database TIEHASH;

    new_RPM__Database(TIEHASH);
    // The dbstruct is used for the C-level rpmlib information on databases
    dbstruct = safemalloc(sizeof(RPM_Database));
    Zero(dbstruct, 1, RPM_Database);
    if (opts)
    {
        if (SvROK(opts) && (SvTYPE(opts) == SVt_PVHV))
        {
            // This is a hash reference. We are concerned only with
            // the keys "root", "mode" and "perms".
            opt_hash = (HV*)SvRV(opts);

            svp = hv_fetch(opt_hash, "root", 4, FALSE);
            if (svp && SvPOK(*svp))
                root = SvPV(*svp, root_len);
            svp = hv_fetch(opt_hash, "mode", 4, FALSE);
            if (svp && SvIOK(*svp))
                mode = SvIV(*svp);
            svp = hv_fetch(opt_hash, "perms", 5, FALSE);
            if (svp && SvIOK(*svp))
                perms = (mode_t)SvIV(*svp);
        }
        else if (SvPOK(opts))
        {
            // They passed a scalar, assumed to be the "root"
            root = SvPV(opts, root_len);
        }
        else
        {
            warn("Wrong type for argument 2 to TIEHASH");
            return ((RPM__Database)NULL);
        }
    }

    // With that all processed, attempt to open the actual RPM DB
    if (rpmdbOpen(root, &dbstruct->dbp, mode, perms) != 0)
    {
        warn("rpmdb_TIEHASH: rpmdbOpen failed");
        return ((RPM__Database)NULL);
    }
    else
    {
        dbstruct->current_rec = 0;
        dbstruct->index_set = (void *)NULL;
    }

    // STRUCT_KEY is used to stash the C-level struct on the TIEHASH obj
    hv_store_nomg(TIEHASH,
                  STRUCT_KEY, STRUCT_KEY_LEN, newSViv((unsigned)dbstruct),
                  FALSE);
    return TIEHASH;
}

RPM__Header rpmdb_FETCH(pTHX_ RPM__Database self, SV* key)
{
    const char* name = NULL; // For the actual name out of (SV *)key
    int namelen;             // Arg for SvPV(..., len)
    int offset;              // In case they pass an integer offset
    Header hdr;              // For rpmdbGetRecord() calls
    SV** svp;
    RPM__Header FETCH;
    RPM_Database* dbstruct;  // This is the struct used to hold C-level data

    // Any successful operation will re-assign this
    FETCH = (RPM__Header)newSVsv(&PL_sv_undef);

    dbstruct_from_object_ret(svp, dbstruct, self, FETCH);
    // De-reference key, if it is a reference
    if (SvROK(key))
        key = SvRV(key);

    // For sake of flexibility (and because it's almost zero overhead),
    // allow the request to be by name -or- by an offset number
    if (SvPOK(key))
    {
        int result;

        name = SvPV(key, namelen);

        // Step 1: Check to see if this has already been requested and is
        // thus cached on the hash itself
        hv_fetch_nomg(svp, self, (char *)name, namelen, FALSE);
        if (svp && SvOK(*svp))
        {
            FETCH = (RPM__Header)SvIV(*svp);
            return FETCH;
        }

        // Create an index set if we don't already have one
        if (! dbstruct->index_set)
        {
            dbstruct->index_set =
                (dbiIndexSet *)safemalloc(sizeof(dbiIndexSet));
            Zero(dbstruct->index_set, 1, dbiIndexSet);
        }
        // Run the search
        result = rpmdbFindPackage(dbstruct->dbp, name, dbstruct->index_set);
        if (result)
        {
            // Some sort of error occured when reading the DB or the
            // name was not found.
            return FETCH;
        }
        else
        {
            // There may have been more than one match, but for now
            // I can only take the first one off the list.
            if (dbstruct->index_set->count)
            {
                offset = dbstruct->index_set->recs[0].recOffset;
            }
            else
            {
                // In theory, this wouldn't happen since zero matches
                // would mean a return value of 1 from the library.
                // But I ain't betting the core on that...
                return FETCH;
            }
        }
    }
    else if (SvIOK(key))
    {
        // This is actually a lot easier than fetch-by-name, which is
        // why I've thrown it in
        offset = SvIV(key);
    }
    else
    {
        warn("RPM::Database::FETCH: Second arg should be name or offset");
        return FETCH;
    }

    hdr = rpmdbGetRecord(dbstruct->dbp, offset);
    // An error results in hdr getting NULL, which is just fine
    if (hdr)
    {
        FETCH = rpmhdr_TIEHASH(aTHX_ sv_2mortal(newSVpv("RPM::Header", 12)),
                               sv_2mortal(newRV((SV *)hdr)),
                               RPM_HEADER_FROM_REF | RPM_HEADER_READONLY);
        // If name is no longer NULL, it means our vector in was a string
        // (key), so put the result back into the hash-cache.
        if (name != NULL)
        {
            hv_store_nomg(self, (char *)name, namelen,
                          newSViv((unsigned)FETCH), FALSE);
        }
    }

    return FETCH;
}

int rpmdb_EXISTS(pTHX_ RPM__Database self, SV* key)
{
    SV* tmp;

    tmp = (SV *)rpmdb_FETCH(aTHX_ self, key);
    // There is probably a cleaner test for (SV *)tmp == PL_sv_undef
    return (SvANY(tmp) != NULL);
}

// This is quite a bit easier than the FIRSTKEY/NEXTKEY combo for headers.
// In these cases, the transition is based on the last offset fetched, which
// we store on the struct part of self. We don't have to worry about an
// iterator struct.
int rpmdb_FIRSTKEY(pTHX_ RPM__Database self, SV** key, RPM__Header* value)
{
    RPM_Database* dbstruct;
    SV** svp;
    AV* tmpav;

    dbstruct_from_object_ret(svp, dbstruct, self, 0);
    // This more or less resets our "iterator"
    dbstruct->current_rec = 0;

    if (! (dbstruct->current_rec = rpmdbFirstRecNum(dbstruct->dbp)))
        return 0;

    *value = rpmdb_FETCH(aTHX_ self, newSViv(dbstruct->current_rec));
    tmpav = rpmhdr_FETCH(aTHX_ *value, newSVpv("name", 4), Nullch, 0, 0);
    svp = av_fetch(tmpav, 0, FALSE);
    *key = newSVsv(*svp);

    return 1;
}

int rpmdb_NEXTKEY(pTHX_ RPM__Database self, SV* key,
                  SV** nextkey, RPM__Header* nextvalue)
{
    RPM_Database* dbstruct;
    SV** svp;
    AV* tmpav;

    dbstruct_from_object_ret(svp, dbstruct, self, 0);

    if (! (dbstruct->current_rec = rpmdbNextRecNum(dbstruct->dbp,
                                                   dbstruct->current_rec)))
        return 0;

    *nextvalue = rpmdb_FETCH(aTHX_ self, newSViv(dbstruct->current_rec));
    tmpav = rpmhdr_FETCH(aTHX_ *nextvalue, newSVpv("name", 4), Nullch, 0, 0);
    svp = av_fetch(tmpav, 0, FALSE);
    *nextkey = newSVsv(*svp);

    return 1;
}

void rpmdb_DESTROY(pTHX_ RPM__Database self)
{
    SV** svp;
    RPM_Database* dbstruct;  // This is the struct used to hold C-level data

    dbstruct_from_object(svp, dbstruct, self);

    rpmdbClose(dbstruct->dbp);
    if (dbstruct->index_set)
        dbiFreeIndexRecord(*dbstruct->index_set);
}

int rpmdb_init(const char* class, const char* root, int perms)
{
    return (1 - rpmdbInit(root, perms));
}

int rpmdb_rebuild(const char* class, const char* root)
{
    return (1 - rpmdbRebuild(root));
}

// This is a front-end to all the rpmdbFindBy*() set, including FindByPackage
// which differs from FETCH above in that if there is actually more than one
// match, all will be returned.
AV* rpmdb_find_by_whatever(pTHX_ RPM__Database self, SV* string, int idx)
{
    const char* str = NULL; // For the actual string out of (SV *)string
    STRLEN len;             // Arg for SvPV(..., len)
    SV** svp;
    RPM_Database* dbstruct; // This is the struct used to hold C-level data
    AV* return_val;
    int result, loop;
    RPM__Header tmp_hdr;
    
    // Any successful operation will store items on this
    return_val = newAV();

    dbstruct_from_object_ret(svp, dbstruct, self, return_val);
    // De-reference key, if it is a reference
    if (SvROK(string))
        string = SvRV(string);
    // Get the string
    str = SvPV(string, len);

    // Create an index set if we don't already have one
    if (! dbstruct->index_set)
    {
        dbstruct->index_set = (dbiIndexSet *)safemalloc(sizeof(dbiIndexSet));
        Zero(dbstruct->index_set, 1, dbiIndexSet);
    }
    // Run the search
    // This goes back to the comment below at the ALIAS: XS keyword, that the
    // indexing here shouldn't be hard-coded.
    if (idx == 0)
        result = rpmdbFindByFile(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == 1)
        result = rpmdbFindByGroup(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == 2)
        result = rpmdbFindByProvides(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == 3)
        result = rpmdbFindByRequiredBy(dbstruct->dbp,str, dbstruct->index_set);
    else if (idx == 4)
        result = rpmdbFindByConflicts(dbstruct->dbp, str, dbstruct->index_set);
    else if (idx == 5)
        result = rpmdbFindPackage(dbstruct->dbp, str, dbstruct->index_set);

    // The various rpmdbFind*() routines return 0 on success
    if (! result)
    {
        av_extend(return_val, dbstruct->index_set->count);
        for (loop = 0; loop < dbstruct->index_set->count; loop++)
        {
            idx = dbstruct->index_set->recs[loop].recOffset;
            tmp_hdr = rpmdb_FETCH(aTHX_ self, sv_2mortal(newSViv(idx)));
            av_store(return_val, loop, sv_2mortal(newSViv((I32)tmp_hdr)));
        }
    }

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

RPM::Header
rpmdb_FETCH(self, key)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmdb_FETCH(aTHX_ self, key);
    OUTPUT:
    RETVAL

int
rpmdb_STORE(self, key, value)
    RPM::Database self;
    SV* key;
    SV* value;
    PROTOTYPE: $$$
    CODE:
    {
        warn("STORE: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
        RETVAL

int
rpmdb_DELETE(self, key)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    {
        warn("DELETE: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
        RETVAL

int
rpmdb_CLEAR(self)
    RPM::Database self;
    PROTOTYPE: $
    CODE:
    {
        warn("CLEAR: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
        RETVAL

int
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
    RPM::Header self;
    PROTOTYPE: $
    PREINIT:
    SV* key;
    SV* value;
    PPCODE:
    {
        RPM__Header hvalue;

        if (! rpmdb_FIRSTKEY(aTHX_ self, &key, &hvalue))
        {
            key = newSVsv(&PL_sv_undef);
            value = newSVsv(&PL_sv_undef);
        }
        else
            value = newRV((SV *)hvalue);

        EXTEND(SP, 2);
        PUSHs(sv_2mortal(value));
        PUSHs(sv_2mortal(newSVsv(key)));
    }

void
rpmdb_NEXTKEY(self, key=NULL)
    RPM::Database self;
    SV* key;
    PROTOTYPE: $;$
    PREINIT:
    SV* nextkey;
    SV* nextvalue;
    PPCODE:
    {
        RPM__Header hvalue;

        if (! rpmdb_NEXTKEY(aTHX_ self, key, &nextkey, &hvalue))
        {
            nextkey = newSVsv(&PL_sv_undef);
            nextvalue = newRV(&PL_sv_undef);
        }
        else
            nextvalue = newRV((SV *)hvalue);

        EXTEND(SP, 2);
        PUSHs(sv_2mortal(nextvalue));
        PUSHs(sv_2mortal(newSVsv(nextkey)));
    }

void
rpmdb_DESTROY(self)
    RPM::Database self;
    PROTOTYPE: $
    CODE:
    rpmdb_DESTROY(self);

int
rpmdb_init(class, root=NULL, perms=O_RDWR)
    const char* class;
    const char* root;
    int perms;
    PROTOTYPE: $;$$
    INIT:
    if (strcmp(class, "RPM::Database"))
        croak("RPM::Database::init must be called as a static method");

int
rpmdb_rebuild(class, root=NULL)
    const char* class;
    const char* root;
    PROTOTYPE: $;$
    INIT:
    if (strcmp(class, "RPM::Database"))
        croak("RPM::Database::rebuild must be called as a static method");

void
rpmdb_find_by_file(self, string)
    RPM::Database self;
    SV* string;
    PROTOTYPE: $$
    ALIAS:
    # These should not be hard-coded, fix in later rev
        find_by_group = 1
        find_by_provides = 2
        find_by_required_by = 3
        find_by_conflicts = 4
        find_by_package = 5
    PPCODE:
    {
        AV* matches;
        int len, size;
        SV** svp;
        RPM__Header hdr;
        SV* hdr_ptr;

        matches = rpmdb_find_by_whatever(aTHX_ self, string, ix);
        if ((len = av_len(matches)) != -1)
        {
            // We have (len+1) elements in the array to put onto the stack
            size = len + 1;
            EXTEND(SP, size);
            while (len >= 0)
            {
                // This being a stack and all, we put them in backwards
                svp = av_fetch(matches, len, FALSE);
                if (svp && SvIOK(*svp))
                {
                    hdr = (RPM__Header)SvIV(*svp);
                    hdr_ptr = sv_bless(newRV_noinc((SV*)hdr),
                                       gv_stashpv("RPM::Header", TRUE));
                    hv_magic(hdr, (GV *)Nullhv, 'P');
                }
                else
                    hdr_ptr = newSVsv(&PL_sv_undef);
                PUSHs(hdr_ptr);
                len--;
            }
        }
        else
            size = 0;

        XSRETURN(size);
    }
