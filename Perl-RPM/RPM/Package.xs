#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Package.xs,v 1.3 2000/10/08 10:09:26 rjray Exp $";

RPM__Package rpmpkg_new(pTHX_ char* class, SV* source, int flags)
{
    char* fname;
    int fname_len;
    RPM__Package retval;
    RPM__Header  pkghdr;

    new_RPM__Package(retval);
    if (! retval) { return retval; }

    if (source && SvROK(source) && (SvTYPE(SvRV(source)) == SVt_PVHV))
    {
        /* This means a hashref of options was passed in for shortcuts' sake */
        SV** svp;
        HV* options = (HV *)SvRV(source);

        svp = hv_fetch(options, "source", 7, FALSE);
        if (SvOK(*svp))
            /* They specified a source in the hashref */
            source = sv_2mortal(*svp);
        else
            /* They, um, didn't */
            source = Nullsv;

        svp = hv_fetch(options, "callback", 9, FALSE);
        if (SvOK(*svp))
            rpmpkg_set_callback(retval, sv_2mortal(*svp));

        svp = hv_fetch(options, "flags", 6, FALSE);
        if (SvOK(*svp) && SvIOK(*svp))
            flags = SvIV(sv_2mortal(*svp));
    }

    if (source && SvOK(source))
    {
        /* Technically, at least one of these tests is a dupe, but necessary
         * since source may have been re-assigned above. */
        if (SvROK(source))
            source = SvRV(source);
        /* If it is a string value, assume it to be a file name */
        if (SvPOK(source))
        {
            fname = SvPV(source, fname_len); 
            retval->path = safemalloc(fname_len + 1);
            strncpy(retval->path, fname, fname_len);
            retval->path[fname_len] = '\0';
            retval->header = rpmhdr_TIEHASH(aTHX_ Nullsv, source,
                                            (flags & RPM_HEADER_MASK));
            if (! retval->header)
            {
                if (retval->callback)
                    SvREFCNT_dec((SV *)retval->callback);
                safefree(retval);
                return 0;
            }
        }
        else
        {
            rpm_error(aTHX_ RPMERR_BADARG,
                      "Argument 2 must be a filename");
            if (retval->callback)
                SvREFCNT_dec((SV *)retval->callback);
            safefree(retval);
            return 0;
        }
    }
    else
    {
        /* Nothing valid in source means an empty object. Process flags. */
        if (flags & RPM_PACKAGE_READONLY)
            retval->readonly = 1;
    }

    return retval;
}

void rpmpkg_DESTROY(pTHX_ RPM__Package self)
{
    /* Free/release the dynamic parts of the package structure */
    if (self->path)
        safefree(self->path);
    /* The Perl structure have their own clean-up methods that will kick in */

    return;
}

SV* rpmpkg_set_callback(pTHX_ RPM__Package self, SV* newcb)
{
    SV* oldcb;

    oldcb = (self->callback) ?
        newRV((SV *)self->callback) : newSVsv(&PL_sv_undef);

    if (SvROK(newcb)) newcb = SvRV(newcb);
    if (SvTYPE(newcb) == SVt_PVCV)
        self->callback = (CV *)newcb;
    else if (SvPOK(newcb))
    {
        char* fn_name;
        char* sv_name;

        sv_name = SvPV(newcb, PL_na);
        if (! strstr(sv_name, "::"))
        {
            Newz(TRUE, fn_name, strlen(sv_name) + 7, char);
            strncat(fn_name, "main::", 6);
            strcat(fn_name + 6, sv_name);
        }
        else
            fn_name = sv_name;

        self->callback = perl_get_cv(fn_name, FALSE);
    }
    else
    {
        self->callback = Nullcv;
    }

    return oldcb;
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

SV* rpmpkg_size(pTHX_ RPM__Package self)
{
    SV* size;

    if (self->header)
        size = rpmhdr_FETCH(self->header, newSVpv("SIZE", 5),
                            /* These are internal params */ Nullch, 0, 0);
    else
        size = newSVsv(&PL_sv_undef);

    return size;
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
rpmpkg_new(class, source=Nullsv, flags=0)
    char* class;
    SV* source;
    int flags;
    PROTOTYPE: $;$
    CODE:
    RETVAL = rpmpkg_new(aTHX_ class, source, flags);
    OUTPUT:
    RETVAL

void
rpmpkg_DESTROY(self)
    RPM::Package self;
    PROTOTYPE: $
    CODE:
    rpmpkg_DESTROY(aTHX_ self);

SV*
rpmpkg_set_callback(self, newcb)
    RPM::Package self;
    SV* newcb;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmpkg_set_callback(aTHX_ self, newcb);
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

SV*
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
