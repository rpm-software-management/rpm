/** \ingroup rpmcli
 * \file lib/poptK.c
 *  Popt tables for signature modes.
 */

#include "system.h"

#include <rpmcli.h>
#include <rpmurl.h>

#include "debug.h"

struct rpmSignArguments_s rpmKArgs =
	{ RESIGN_CHK_SIGNATURE, CHECKSIG_ALL, 0, NULL };

#define	POPT_ADDSIGN		1005
#define	POPT_RESIGN		1006

/**
 */
static void signArgCallback( /*@unused@*/ poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg,
		/*@unused@*/ const void * data)
	/*@modifies rpmKArgs @*/
{
    struct rpmSignArguments_s * rka = &rpmKArgs;

    switch (opt->val) {
    case 'K':
	rka->addSign = RESIGN_CHK_SIGNATURE;
	rka->sign = 0;
	break;

    case POPT_RESIGN:
	rka->addSign = RESIGN_NEW_SIGNATURE;
	rka->sign = 1;
	break;

    case POPT_ADDSIGN:
	rka->addSign = RESIGN_ADD_SIGNATURE;
	rka->sign = 1;
	break;
    }
}

/**
 */
struct poptOption rpmSignPoptTable[] = {
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
	signArgCallback, 0, NULL, NULL },
 { "addsign", '\0', 0, 0, POPT_ADDSIGN,
	N_("add a signature to a package"), NULL },
 { "resign", '\0', 0, 0, POPT_RESIGN,
	N_("sign a package (discard current signature)"), NULL },
 { "sign", '\0', 0, &rpmKArgs.sign, 0,
	N_("generate GPG/PGP signature"), NULL },
 { "checksig", 'K', 0, 0, 'K',
	N_("verify package signature"), NULL },
 { "nogpg", '\0', POPT_BIT_CLR,
	&rpmKArgs.checksigFlags, CHECKSIG_GPG,
	N_("skip any GPG signatures"), NULL },
 { "nopgp", '\0', POPT_BIT_CLR|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmKArgs.checksigFlags, CHECKSIG_PGP,
	N_("skip any PGP signatures"), NULL },
 { "nomd5", '\0', POPT_BIT_CLR,
	&rpmKArgs.checksigFlags, CHECKSIG_MD5,
	N_("do not verify file md5 checksums"), NULL },

   POPT_TABLEEND
};
