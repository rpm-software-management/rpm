/*
 * $Id: RPM.h,v 1.17 2002/01/23 00:58:48 jbj Exp $
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

/* XXX Certain constants moved in rpm-4.0.3 to a developing CLI API */
#if RPM_MAJOR >= 4 && RPM_MINOR >= 0 && RPM_PATCH >= 3
#include <rpmcli.h>
#endif

#include <rpmlib.h>
#if RPM_MAJOR < 4
#  include <header.h>
#  include <dbindex.h>
#endif

/* Various flags. For now, one nybble for header and one for package. */
#define RPM_HEADER_MASK        0x0f
#define RPM_HEADER_READONLY    0x01
#define RPM_HEADER_FROM_REF    0x02

#define RPM_PACKAGE_MASK       0x0f00
#define RPM_PACKAGE_READONLY   0x0100
#define RPM_PACKAGE_NOREAD     0x0200

/*
  Use this define for deriving the saved underlying struct, rather than coding
  it a dozen places.
*/
#define struct_from_object_ret(type, header, object, err_ret) \
    { \
        MAGIC* mg = mg_find((SV *)(object), '~'); \
        if (mg) \
            (header) = (type *)SvIV(mg->mg_obj); \
        else \
            return (err_ret); \
    }
/* And a no-return version: */
#define struct_from_object(type, header, object) \
    { \
        MAGIC* mg = mg_find((SV *)(object), '~'); \
        if (mg) \
            (header) = (type *)SvIV(mg->mg_obj); \
        else \
            (header) = Null(type *); \
    }

#define new_RPM_storage(type) (type *)safemalloc(sizeof(type))

/*
 *    Perl complement: RPM::Database
 */

/*
  This is the underlying struct that implements the interface to the RPM
  database.
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
    /* This HV will be used to cache key/value pairs to avoid re-computing */
    HV* storage;
} RPM_Database;

typedef HV* RPM__Database;


/*
 *    Perl complement: RPM::Header
 */

/*
  This is the underlying struct that implements the interface to the RPM
  headers.
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
    /* This HV will be used to cache key/value pairs to avoid re-computing */
    HV* storage;
    /* Keep a per-header iterator for things like FIRSTKEY and NEXTKEY */
    HeaderIterator iterator;
    int read_only;
    /* Since we close the files after reading, store the filename here in case
       we have to re-open it later */
    char* source_name;
} RPM_Header;

typedef HV* RPM__Header;


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
    char* path;
    /* A weak ref to the header structure for the package, if it exists */
    RPM__Header header;
    /* The RPM signature (if present) is stored in the same struct as hdrs */
    Header signature;
    /* Should this be treated as a read-only source? */
    int readonly;
    /* The current notify/callback function associated with this package */
    CV* callback;
    /* Any data they want to have passed to the callback */
    SV* cb_data;
} RPM_Package;

typedef RPM_Package* RPM__Package;


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
extern RPM__Header rpmhdr_TIEHASH(pTHX_ char *, SV *, int);
extern SV* rpmhdr_FETCH(pTHX_ RPM__Header, SV *, const char *, int, int);
extern int rpmhdr_STORE(pTHX_ RPM__Header, SV *, SV *);
extern int rpmhdr_DELETE(pTHX_ RPM__Header, SV *);
extern bool rpmhdr_EXISTS(pTHX_ RPM__Header, SV *);
extern unsigned int rpmhdr_size(pTHX_ RPM__Header);
extern int rpmhdr_tagtype(pTHX_ RPM__Header, SV *);
extern int rpmhdr_write(pTHX_ RPM__Header, SV *, int);
extern int rpmhdr_is_source(pTHX_ RPM__Header);
extern int rpmhdr_cmpver(pTHX_ RPM__Header, RPM__Header);
extern int rpmhdr_scalar_tag(pTHX_ SV*, int);

/* RPM/Database.xs: */
extern RPM__Database rpmdb_TIEHASH(pTHX_ char *, SV *);
extern SV* rpmdb_FETCH(pTHX_ RPM__Database, SV *);
extern bool rpmdb_EXISTS(pTHX_ RPM__Database, SV *);

/* RPM/Package.xs: */
extern RPM__Package rpmpkg_new(pTHX_ char *, SV *, int);
extern SV* rpmpkg_set_callback(pTHX_ RPM__Package, SV *);
extern int rpmpkg_is_source(pTHX_ RPM__Package);
extern int rpmpkg_cmpver_pkg(pTHX_ RPM__Package, RPM__Package);
extern int rpmpkg_cmpver_hdr(pTHX_ RPM__Package, RPM__Header);

#endif /* H_RPM_XS_HDR */
