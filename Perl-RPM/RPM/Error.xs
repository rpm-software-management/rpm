#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Error.xs,v 1.2 2000/10/05 04:48:59 rjray Exp $";

static CV* err_callback;

/*
  This was static, but it needs to be accessible from other modules, as well.
*/
SV* rpm_errSV;

/*
  This is a callback routine that the bootstrapper will register with the RPM
  lib so as to catch any errors. (I hope)
*/
static void rpm_catch_errors(void)
{
    /* Because rpmErrorSetCallback expects (void)fn(void), we have to declare
       our thread context here */
    dTHX;
    int error_code;
    char* error_string;

    error_code = rpmErrorCode();
    error_string = rpmErrorString();

    /* Set the string part, first */
    sv_setsv(rpm_errSV, newSVpv(error_string, strlen(error_string)));
    /* Set the IV part */
    sv_setiv(rpm_errSV, error_code);
    /* Doing that didn't erase the PV part, but it cleared the flag: */
    SvPOK_on(rpm_errSV);

    /* If there is a current callback, invoke it: */
    if (err_callback != Nullcv)
    {
        /* This is just standard boilerplate for calling perl from C */
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(sp);
        XPUSHs(sv_2mortal(newSViv(error_code)));
        XPUSHs(sv_2mortal(newSVpv(error_string, strlen(error_string))));
        PUTBACK;

        /* The actual call */
        perl_call_sv((SV *)err_callback, G_DISCARD);

        /* More boilerplate */
        SPAGAIN;
        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    return;
}

/* This is just to offer an easy way to clear both sides of $RPM::err */
void clear_errors(pTHX)
{
    sv_setsv(rpm_errSV, newSVpv("", 0));
    sv_setiv(rpm_errSV, 0);
    SvPOK_on(rpm_errSV);

    return;
}

SV* set_error_callback(pTHX_ SV* newcb)
{
    SV* oldcb;

    oldcb = (err_callback) ? newRV((SV *)err_callback) : newSVsv(&PL_sv_undef);

    if (SvROK(newcb)) newcb = SvRV(newcb);
    if (SvTYPE(newcb) == SVt_PVCV)
        err_callback = (CV *)newcb;
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

        err_callback = perl_get_cv(fn_name, FALSE);
    }
    else
    {
        err_callback = Nullcv;
    }

    return oldcb;
}

void rpm_error(pTHX_ int code, const char* message)
{
    rpmError(code, (char *)message);
}


MODULE = RPM::Error     PACKAGE = RPM::Error           


SV*
set_error_callback(newcb)
    SV* newcb;
    PROTOTYPE: $
    CODE:
    RETVAL = set_error_callback(aTHX_ newcb);
    OUTPUT:
    RETVAL

void
clear_errors()
    PROTOTYPE:
    CODE:
    clear_errors(aTHX);

void
rpm_error(code, message)
    int code;
    char* message;
    PROTOTYPE: $$
    CODE:
    rpm_error(aTHX_ code, message);


BOOT:
{
    rpm_errSV = perl_get_sv("RPM::err", GV_ADD|GV_ADDMULTI);
    rpmErrorSetCallback(rpm_catch_errors);
    err_callback = Nullcv;
}
