/*
 * $Id: RPM.h,v 1.1 2000/05/22 08:37:23 rjray Exp $
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

extern const char* sv2key(SV *);
extern int tag2num(const char *);
extern const char* num2tag(int);

typedef struct {
    rpmdb dbp;
    int current_rec;
    dbiIndexSet* index_set;
} RPM_Database;

typedef HV* RPM__Database;

#define new_RPM__Database(x) x = newHV()

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

#endif /* H_RPM_XS_HDR */
