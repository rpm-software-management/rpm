#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Package.xs,v 1.1 2000/08/06 08:57:09 rjray Exp $";

/*
  Use this define for deriving the saved Package struct, rather than coding
  it a dozen places. Note that the hv_fetch call is the no-magic one defined
  in RPM.h
*/
#define package_from_object_ret(s_ptr, package, object, err_ret) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (package) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Package *)SvIV(*(s_ptr)) : NULL; \
    if (! (package)) \
        return (err_ret);
/* And a no-return-value version: */
#define package_from_object(s_ptr, package, object) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (package) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Package *)SvIV(*(s_ptr)) : NULL;


/* These three are for reading the package data from external sources */
static int new_from_fd_t(FD_t fd, RPM_Package* new_pkg)
{
    return 1;
}

static int new_from_fd(int fd, RPM_Package* new_pkg)
{
    FD_t FD = fdDup(fd);

    return(new_from_fd_t(FD, new_pkg));
}

static int new_from_fname(const char* source, RPM_Package* new_pkg)
{
    FD_t fd;

    if (! (fd = Fopen(source, "r+")))
        return 0;

    return(new_from_fd_t(fd, new_pkg));
}

RPM_Package* rpmpkg_new(pTHX_ SV* package)
{
}

void rpmpkg_DESTROY(pTHX_ RPM_Package* self)
{
}

unsigned int rpmpkg_size(pTHX_ RPM_Package* self)
{
    SV** svp;
    RPM_Package* pkg;

    package_from_object_ret(svp, pkg, self, 0);

}

int rpmpkg_write(pTHX_ RPM_Package* self, SV* gv_in, int magicp)
{
    IO* io;
    PerlIO* fp;
    FD_t fd;
    RPM_Package* pkg;
    GV* gv;
    SV** svp;
    int written = 0;

    gv = (SvPOK(gv_in) && (SvTYPE(gv_in) == SVt_PVGV)) ?
        (GV *)SvRV(gv_in) : (GV *)gv_in;
    package_from_object_ret(svp, pkg, self, 0);

    if (!gv || !(io = GvIO(gv)) || !(fp = IoIFP(io)))
        return written;

    return written;
}

/* T/F test whether the package references a SRPM */
int rpmpkg_is_source(pTHX_ RPM_Package* self)
{
    SV** svp;
    RPM_Package* pkg;

    package_from_object_ret(svp, pkg, self, 0);
    return (rpmhdr_is_source(aTHX_ pkg->header));
}

/*
  A classic-style comparison function for two packages, returns -1 if a < b,
  1 if a > b, and 0 if a == b. In terms of version/release, that is.
*/
int rpmpkg_cmpver(pTHX_ RPM_Package* self, RPM_Package* other)
{
    RPM_Package* one;
    RPM_Package* two;
    SV** svp;

    package_from_object(svp, one, self);
    if (! ((one) && (one->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 1 has no header data");
        return 0;
    }
    package_from_object(svp, two, other);
    if (! ((two) && (two->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 2 has no header data");
        return 0;
    }

    return rpmhdr_cmpver(aTHX_ one->header, two->header);
}

/*
  This is essentially a duplicate of the above, to provide an interface-
  polymorphic cmpver method to the user. This version takes a RPM::Header
  object as arg 2.
*/
int rpmpkg_cmpver2(pTHX_ RPM_Package* self, RPM__Header two)
{
    RPM_Package* one;
    SV** svp;

    package_from_object(svp, one, self);
    if (! ((one) && (one->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 1 has no header data");
        return 0;
    }

    return rpmhdr_cmpver(aTHX_ one->header, two);
}


MODULE = RPM::Package   PACKAGE = RPM::Package          PREFIX = rpmpkg_


void
rpmpkg_DESTROY(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    rpmpkg_DESTROY(aTHX_ self);

unsigned int
rpmpkg_size(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmpkg_size(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmpkg_write(self, gv)
    RPM::Package self;
    SV* gv;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmpkg_write(aTHX_ self, gv);
    OUTPUT:
    RETVAL

int
rpmpkg_is_source(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmpkg_is_source(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmpkg_cmpver(self, other)
    RPM::Package self;
    SV* other;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmpkg_cmpver(aTHX_ self, other);
    OUTPUT:
    RETVAL
