#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: RPM.xs,v 1.8 2000/10/13 09:24:05 rjray Exp $";

extern XS(boot_RPM__Constants);
extern XS(boot_RPM__Header);
extern XS(boot_RPM__Database);
extern XS(boot_RPM__Error);
/*extern XS(boot_RPM__Package);*/

static HV* tag2num_priv;
static HV* num2tag_priv;

static void setup_tag_mappings(pTHX)
{
    const char* tag;
    int num;
    int idx;
    char str_num[8];

    tag2num_priv = perl_get_hv("RPM::tag2num", TRUE);
    num2tag_priv = perl_get_hv("RPM::num2tag", TRUE);
    for (idx = 0; idx < rpmTagTableSize; idx++)
    {
        /*
          For future reference: The offset of 7 used in referring to the
          (const char *) tag and its length is to discard the "RPMTAG_"
          prefix inherent in the tag names.
        */
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

int tag2num(pTHX_ const char* tag)
{
    SV** svp;

    /* Get the #define value for the tag from the hash made at boot-up */
    svp = hv_fetch(tag2num_priv, (char *)tag, strlen(tag), FALSE);
    if (! (svp && SvOK(*svp) && SvIOK(*svp)))
        /* Later we may need to set some sort of error message */
        return 0;

    return (SvIV(*svp));
}

const char* num2tag(pTHX_ int num)
{
    SV** svp;
    char str_num[8];
    SV* tmp;

    Zero(str_num, 1, 8);
    snprintf(str_num, 8, "%d", num);
    svp = hv_fetch(num2tag_priv, str_num, strlen(str_num), FALSE);
    if (! (svp && SvPOK(*svp)))
        return Nullch;

    return (SvPV(*svp, PL_na));
}

char* rpm_rpm_osname(void)
{
    char* os_name;
    int os_val;

    rpmGetOsInfo((const char **)&os_name, &os_val);
    return os_name;
}

char* rpm_rpm_archname(void)
{
    char* arch_name;
    int arch_val;

    rpmGetArchInfo((const char **)&arch_name, &arch_val);
    return arch_name;
}

MODULE = RPM            PACKAGE = RPM           PREFIX = rpm_


char*
rpm_rpm_osname()
    PROTOTYPE:

char*
rpm_rpm_archname()
    PROTOTYPE:


BOOT:
{
    SV * config_loaded;

    config_loaded = perl_get_sv("RPM::__config_loaded", GV_ADD|GV_ADDMULTI);
    if (! (SvOK(config_loaded) && SvTRUE(config_loaded)))
    {
        rpmReadConfigFiles(NULL, NULL);
        sv_setiv(config_loaded, TRUE);
    }

    setup_tag_mappings(aTHX);

    newXS("RPM::bootstrap_Constants", boot_RPM__Constants, file);
    newXS("RPM::bootstrap_Header", boot_RPM__Header, file);
    newXS("RPM::bootstrap_Database", boot_RPM__Database, file);
    newXS("RPM::bootstrap_Error", boot_RPM__Error, file);
    /*newXS("RPM::bootstrap_Package", boot_RPM__Package, file);*/
}
