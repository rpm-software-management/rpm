/*
 * $Id: RPM.h,v 1.2 2000/05/27 05:22:51 rjray Exp $
 *
 * Various C-specific decls/includes/etc. for the RPM linkage
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifndef H_RPM_XS_HDR
#define H_RPM_XS_HDR

#ifdef Stat
#  undef Stat
#endif
#ifdef Mkdir
#  undef Mkdir
#endif
#ifdef Fstat
#  undef Fstat
#endif
#ifdef Fflush
#  undef Fflush
#endif

// Borrowed from DB_File.xs
#ifndef pTHX
#    define pTHX
#    define pTHX_
#    define aTHX
#    define aTHX_
#endif

#ifndef newSVpvn
#    define newSVpvn(a,b)       newSVpv(a,b)
#endif

#include <rpm/rpmlib.h>
#include <rpm/header.h>
#include <rpm/dbindex.h>

//
// This is the underlying struct that implements the interface to the RPM
// database. Since we need the actual object to be a hash, the struct will
// be stored as an SV (actually, a pointer to a struct) on a special key
// defined below.
//

typedef struct {
    rpmdb dbp;
    int current_rec;
    dbiIndexSet* index_set;
} RPM_Database;

typedef HV* RPM__Database;

#define new_RPM__Database(x) x = newHV()

//
// This is the underlying struct that implements the interface to the RPM
// headers. As above, we need the actual object to be a hash, so the struct
// will be stored as an SV on the same sort of special key as RPM__Database
// uses.
//

typedef struct {
    Header hdr;
    // These three tags will probably cover at leas 80% of data requests
    const char* name;
    const char* version;
    const char* release;
    // These are set by rpmReadPackageHeader when applicable
    int isSource;   // If this header is for a source RPM (SRPM)
    int major;      // Major and minor rev numbers of package's format
    int minor;
    // Keep a per-header iterator for things like FIRSTKEY and NEXTKEY
    HeaderIterator iterator;
    int read_only;
} RPM_Header;

typedef HV* RPM__Header;

#define new_RPM__Header(x) x = newHV()

#define RPM_HEADER_READONLY 1
#define RPM_HEADER_FROM_REF 2

// Because the HV* are going to be set magical, the following is needed for
// explicit fetch and store calls that are done within the tied FETCH/STORE
#define hv_fetch_nomg(SVP, h, k, kl, f) \
        SvMAGICAL_off((SV *)(h)); \
        (SVP) = hv_fetch((h), (k), (kl), (f)); \
        SvMAGICAL_on((SV *)(h))
#define hv_store_nomg(h, k, kl, v, f) \
        SvMAGICAL_off((SV *)(h)); \
        hv_store((h), (k), (kl), (v), (f)); \
        SvMAGICAL_on((SV *)(h))

//
// This silly-looking key is what will be used on the RPM::Header and
// RPM::Database objects to stash the underlying struct.
//
#define STRUCT_KEY "<<<struct>>>"
// This must match!
#define STRUCT_KEY_LEN 13

//
// This struct will be used for RPM data type coming out of an RPM::Header
// object. It will be implemented as a tied scalar, so we shouldn't need any
// weird private-key voodoo like for the two previous.
//

typedef struct {
    SV* value;    // May be an SV* or a ptr to an AV*
    int size;     // Number of items
    int type;     // Will match one of the RPM_*_TYPE values.
} RPM_Header_datum;

typedef RPM_Header_datum* RPM__Header__datum;

#define new_RPM__Header__datum(x) Newz(TRUE, (x), 1, RPM_Header_datum)

//
// These represent the various interfaces that are allowed for use outside
// their native modules.
//
// RPM.xs:
extern int tag2num(const char *);
extern const char* num2tag(int);
extern void clear_errors(void);
extern SV* set_error_callback(SV *);
extern void rpm_error(int, const char *);

// RPM/Header.xs:
extern const char* sv2key(SV *);
extern RPM__Header rpmhdr_TIEHASH(SV *, SV *, int);
extern AV* rpmhdr_FETCH(RPM__Header, SV *, const char *, int, int);
extern int rpmhdr_STORE(RPM__Header, SV *, AV *);
extern int rpmhdr_DELETE(RPM__Header, SV *);
extern int rpmhdr_EXISTS(RPM__Header, SV *);
extern unsigned int rpmhdr_size(RPM__Header);
extern int rpmhdr_tagtype(RPM__Header, SV *);
extern int rpmhdr_write(RPM__Header, SV *, int);

// RPM/Database.xs:
extern RPM__Database rpmdb_TIEHASH(char *, SV *);
extern RPM__Header rpmdb_FETCH(RPM__Database, SV *);
extern int rpmdb_EXISTS(RPM__Database, SV *);

#endif /* H_RPM_XS_HDR */
