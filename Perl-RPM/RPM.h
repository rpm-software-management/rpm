/*
 * $Id: RPM.h,v 1.10 2000/10/05 04:48:59 rjray Exp $
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
#ifdef Fopen
#  undef Fopen
#endif

/* Borrowed from DB_File.xs */
#ifndef pTHX
#    define pTHX
#    define pTHX_
#    define aTHX
#    define aTHX_
#    define dTHX dTHR
#endif

#ifndef newSVpvn
#    define newSVpvn(a,b)       newSVpv(a,b)
#endif

#include <rpm/rpmlib.h>
#if RPM_MAJOR < 4
#  include <rpm/header.h>
#  include <rpm/dbindex.h>
#endif

/*
 *    Perl complement: RPM::Database
 */

/*
  This is the underlying struct that implements the interface to the RPM
  database. Since we need the actual object to be a hash, the struct will
  be stored as an SV (actually, a pointer to a struct) on a special key
  defined below.
*/

typedef struct {
    rpmdb dbp;
    int current_rec;
#if RPM_MAJOR < 4
    dbiIndexSet* index_set;
#else
    int noffs;
    int offx;
    int* offsets;
#endif
} RPM_Database;

typedef HV* RPM__Database;

#define new_RPM__Database(x) x = newHV()


/*
 *    Perl complement: RPM::Header
 */

/*
  This is the underlying struct that implements the interface to the RPM
  headers. As above, we need the actual object to be a hash, so the struct
  will be stored as an SV on the same sort of special key as RPM__Database
  uses.
*/

typedef struct {
    Header hdr;
    /* These three tags will probably cover at least 80% of data requests */
    const char* name;
    const char* version;
    const char* release;
    /* These are set by rpmReadPackageHeader when applicable */
    int isSource;   /* If this header is for a source RPM (SRPM) */
    int major;      /* Major and minor rev numbers of package's format */
    int minor;
    /* Keep a per-header iterator for things like FIRSTKEY and NEXTKEY */
    HeaderIterator iterator;
    int read_only;
    /* Since we close the files after reading, store the filename here in case
       we have to re-open it later */
    char* source_name;
} RPM_Header;

typedef HV* RPM__Header;

#define new_RPM__Header(x) x = newHV()

#define RPM_HEADER_READONLY 1
#define RPM_HEADER_FROM_REF 2


/*
 *    Perl complement: RPM::Package
 */

/*
  This is the underlying struct that implements the interface to the RPM
  packages. As above, we need the actual object to be a hash, so the struct
  will be stored as an SV on the same sort of special key as RPM__Database
  and RPM__Header use.
*/

typedef struct {
    /* The filepath, ftp path or URI that refers to the package */
    const char* path;
    /* A weak ref to the header structure for the package, if it exists */
    RPM__Header header;
    /* The RPM signature (if present) is stored in the same struct as hdrs */
    Header signature;
    /* Should this be treated as a read-only source? */
    int read_only;
    /* The current notify/callback function associated with this package */
    CV* callback;
} RPM_Package;

typedef RPM_Package* RPM__Package;

#define new_RPM__Package(x) x = (RPM__Package)safemalloc(sizeof(RPM_Package))


/*
  Because our HV* are going to be set magical, the following is needed for
  explicit fetch and store calls that are done within the tied FETCH/STORE
  methods.
*/
#define hv_fetch_nomg(SVP, h, k, kl, f) \
        SvMAGICAL_off((SV *)(h)); \
        (SVP) = hv_fetch((h), (k), (kl), (f)); \
        SvMAGICAL_on((SV *)(h))
#define hv_store_nomg(h, k, kl, v, f) \
        SvMAGICAL_off((SV *)(h)); \
        hv_store((h), (k), (kl), (v), (f)); \
        SvMAGICAL_on((SV *)(h))

/*
  This silly-looking key is what will be used on the RPM::Header and
  RPM::Database objects to stash the underlying struct.
*/
#define STRUCT_KEY "<<<struct>>>"
/* This must match! */
#define STRUCT_KEY_LEN 13

/*
  These represent the various interfaces that are allowed for use outside
  their native modules.
*/
/* RPM.xs: */
extern int tag2num(pTHX_ const char *);
extern const char* num2tag(pTHX_ int);

/* RPM/Error.xs: */
extern SV* rpm_errSV;
extern void clear_errors(pTHX);
extern SV* set_error_callback(pTHX_ SV *);
extern void rpm_error(pTHX_ int, const char *);

/* RPM/Header.xs: */
extern const char* sv2key(pTHX_ SV *);
extern RPM__Header rpmhdr_TIEHASH(pTHX_ SV *, SV *, int);
extern SV* rpmhdr_FETCH(pTHX_ RPM__Header, SV *, const char *, int, int);
extern int rpmhdr_STORE(pTHX_ RPM__Header, SV *, SV *);
extern int rpmhdr_DELETE(pTHX_ RPM__Header, SV *);
extern int rpmhdr_EXISTS(pTHX_ RPM__Header, SV *);
extern unsigned int rpmhdr_size(pTHX_ RPM__Header);
extern int rpmhdr_tagtype(pTHX_ RPM__Header, SV *);
extern int rpmhdr_write(pTHX_ RPM__Header, SV *, int);
extern int rpmhdr_is_source(pTHX_ RPM__Header);
extern int rpmhdr_cmpver(pTHX_ RPM__Header, RPM__Header);
extern int rpmhdr_scalar_tag(pTHX_ SV*, int);

/* RPM/Database.xs: */
extern RPM__Database rpmdb_TIEHASH(pTHX_ char *, SV *);
extern RPM__Header rpmdb_FETCH(pTHX_ RPM__Database, SV *);
extern int rpmdb_EXISTS(pTHX_ RPM__Database, SV *);

/* RPM/Package.xs: */
extern int rpmpkg_is_source(pTHX_ RPM_Package *);
extern int rpmpkg_cmpver(pTHX_ RPM_Package *, RPM_Package *);
extern int rpmpkg_cmpver2(pTHX_ RPM_Package *, RPM__Header);

#endif /* H_RPM_XS_HDR */
