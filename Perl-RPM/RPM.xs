#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: RPM.xs,v 1.1 2000/05/22 08:37:45 rjray Exp $";

extern XS(boot_RPM__Constants);
extern XS(boot_RPM__Header);
extern XS(boot_RPM__Database);

static HV* tag2num_priv;
static HV* num2tag_priv;

void setup_tag_mappings(void)
{
    const char* tag;
    int num;
    int idx;
    char str_num[8];

    tag2num_priv = perl_get_hv("RPM::tag2num", TRUE);
    num2tag_priv = perl_get_hv("RPM::num2tag", TRUE);
    for (idx = 0; idx < rpmTagTableSize; idx++)
    {
        //
        // For future reference: The offset of 7 used in referring to the
        // (const char *) tag and its length is to discard the "RPMTAG_"
        // prefix inherent in the tag names.
        //
        tag = rpmTagTable[idx].name;
        num = rpmTagTable[idx].val;
        hv_store(tag2num_priv, (char *)tag + 7, strlen(tag) - 7,
                 newSViv(num), FALSE);
        Zero(str_num, 1, 8);
        snprintf(str_num, 8, "%d", num);
        hv_store(num2tag_priv, str_num, strlen(str_num),
                 newSVpv((char *)tag + 7, strlen(tag) - 7), FALSE);
    }
}

// This is a callback routine that the bootstrapper will register with the RPM
// lib so as to catch any errors. (I hope)
void rpm_catch_errors_SV(void)
{
    SV* errSV;
    int error_code;
    char* error_string;

    error_code = rpmErrorCode();
    error_string = rpmErrorString();
    errSV = perl_get_sv("RPM::err", GV_ADD|GV_ADDMULTI);

    // Set the string part, first
    sv_setsv(errSV, newSVpv(error_string, strlen(error_string)));
    // Set the IV part
    sv_setiv(errSV, error_code);
    // Doing that didn't erase the PV part, but it cleared the flag:
    SvPOK_on(errSV);

    return;
}

// This is just to make available an easy way to clear both sides of $RPM::err
void rpm_clear_errors_SV(void)
{
    SV* errSV;

    errSV = perl_get_sv("RPM::err", GV_ADD|GV_ADDMULTI);
    sv_setsv(errSV, newSVpv("", 0));
    sv_setiv(errSV, 0);
    SvPOK_on(errSV);

    return;
}

int tag2num(const char* tag)
{
    SV** svp;

    // Get the #define value for the tag from the hash made at boot-up
    svp = hv_fetch(tag2num_priv, (char *)tag, strlen(tag), FALSE);
    if (! (svp && SvOK(*svp) && SvIOK(*svp)))
        // Later we may need to set some sort of error message
        return 0;

    return (SvIV(*svp));
}

const char* num2tag(int num)
{
    SV** svp;
    STRLEN na;
    char str_num[8];
    SV* tmp;

    Zero(str_num, 1, 8);
    snprintf(str_num, 8, "%d", num);
    svp = hv_fetch(num2tag_priv, str_num, strlen(str_num), FALSE);
    if (! (svp && SvPOK(*svp)))
        return Nullch;

    return (SvPV(*svp, na));
}


MODULE = RPM            PACKAGE = RPM           
PROTOTYPES: DISABLE

BOOT:
{
    SV * config_loaded;

    config_loaded = perl_get_sv("RPM::__config_loaded", GV_ADD|GV_ADDMULTI);
    if (! (SvOK(config_loaded) && SvTRUE(config_loaded)))
    {
        rpmReadConfigFiles(NULL, NULL);
        sv_setiv(config_loaded, TRUE);
    }

    setup_tag_mappings();
    rpmErrorSetCallback(rpm_catch_errors_SV);

    newXS("RPM::bootstrap_Constants", boot_RPM__Constants, file);
    newXS("RPM::bootstrap_Header", boot_RPM__Header, file);
    newXS("RPM::bootstrap_Database", boot_RPM__Database, file);
}
