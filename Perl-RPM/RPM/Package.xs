#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Package.xs,v 1.2 2000/10/05 04:48:59 rjray Exp $";

/* These three are for reading the package data from external sources */
static int new_from_fd_t(FD_t fd, RPM__Package new_pkg)
{
    return 1;
}

static int new_from_fd(int fd, RPM__Package new_pkg)
{
    FD_t FD = fdDup(fd);

    return(new_from_fd_t(FD, new_pkg));
}

static int new_from_fname(const char* source, RPM__Package new_pkg)
{
    FD_t fd;

    if (! (fd = Fopen(source, "r+")))
        return 0;

    return(new_from_fd_t(fd, new_pkg));
}

RPM__Package rpmpkg_new(pTHX_ char* class, SV* source)
{
}

void rpmpkg_DESTROY(pTHX_ RPM__Package self)
{
    /* Free/release the dynamic parts of the package structure */
    rpmhdr_DESTROY(aTHX_ self->header);
}

int rpmpkg_install(pTHX_ RPM__Package self)
{
}

int rpmpkg_uninstall(pTHX_ RPM__Package self)
{
}

int rpmpkg_verify(pTHX_ RPM__Package self)
{
}

unsigned int rpmpkg_size(pTHX_ RPM__Package self)
{
    if (self->header)
    {
        SV* size = rpmhdr_FETCH(self->header, newSVpv("SIZE", 5),
                                /* These are internal params */ Nullch, 0, 0);

        return (SvIV(size));
    }
    else
        return 0;
}

/* T/F test whether the package references a SRPM */
int rpmpkg_is_source(pTHX_ RPM__Package self)
{
    return (self->header) ? (rpmhdr_is_source(aTHX_ self->header)) : 0;
}

/*
  A classic-style comparison function for two packages, returns -1 if a < b,
  1 if a > b, and 0 if a == b. In terms of version/release, that is.
*/
int rpmpkg_cmpver_pkg(pTHX_ RPM__Package self, RPM__Package other)
{
    if (! ((self) && (self->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 1 has no header data");
        return 0;
    }
    if (! ((other) && (other->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 2 has no header data");
        return 0;
    }

    return rpmhdr_cmpver(aTHX_ self->header, other->header);
}

/*
  This is essentially a duplicate of the above, to provide an interface-
  polymorphic cmpver method to the user. This version takes a RPM::Header
  object as arg 2.
*/
int rpmpkg_cmpver_hdr(pTHX_ RPM__Package self, RPM__Header two)
{
    if (! ((self) && (self->header)))
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Package::rpmpkg_cmpver: Arg 1 has no header data");
        return 0;
    }

    return rpmhdr_cmpver(aTHX_ self->header, two);
}


MODULE = RPM::Package   PACKAGE = RPM::Package          PREFIX = rpmpkg_


RPM::Package
rpmpkg_new(class, source=Nullsv)
    char* class;
    SV* source;
    PROTOTYPE: $;$
    CODE:
    RETVAL = rpmpkg_new(aTHX_ class, source);
    OUTPUT:
    RETVAL

void
rpmpkg_DESTROY(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    rpmpkg_DESTROY(aTHX_ self);

int
rpmpkg_install(self, ...)
    RPM::Package self;
    PROTOTYPE: $@
    CODE:
    RETVAL = rpmpkg_install(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmpkg_uninstall(self, ...)
    RPM::Package self;
    PROTOTYPE: $@
    CODE:
    RETVAL = rpmpkg_uninstall(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmpkg_verify(self, ...)
    RPM::Package self;
    PROTOTYPE: $@
    CODE:
    RETVAL = rpmpkg_verify(aTHX_ self);
    OUTPUT:
    RETVAL

unsigned int
rpmpkg_size(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmpkg_size(aTHX_ self);
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

RPM::Header
rpmpkg_header(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    RETVAL = self->header;
    OUTPUT:
    RETVAL

int
rpmpkg_cmpver(self, other)
    RPM::Package self;
    SV* other;
    PROTOTYPE: $$
    PREINIT:
    RPM__Package pkg2;
    RPM__Header hdr2;
    CODE:
    {
        if (sv_derived_from(other, "RPM::Package"))
        {
            IV tmp = SvIV((SV*)SvRV(other));
            pkg2 = (RPM__Package)tmp;

            RETVAL = rpmpkg_cmpver_pkg(aTHX_ self, pkg2);
        }
        else if (sv_derived_from(other, "RPM::Header"))
        {
            hdr2 = (RPM__Header)SvRV(other);

            RETVAL = rpmpkg_cmpver_hdr(aTHX_ self, hdr2);
        }
        else
        {
            rpm_error(aTHX_ RPMERR_BADARG,
                      "RPM::Package::cmpver: Arg 2 is not of a compatible "
                      "type (should be either RPM::Header or RPM::Package");
            RETVAL = 0;
        }
    }
    OUTPUT:
    RETVAL
