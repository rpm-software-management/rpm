/** \ingroup rpmcli
 * \file lib/poptK.c
 *  Popt tables for signature modes.
 */

#include "system.h"

#include <rpmcli.h>

#include "debug.h"

struct rpmSignArguments_s rpmKArgs =
	{ RPMSIGN_NONE, CHECKSIG_ALL, 0, NULL };

/**
 */
/*@unchecked@*/
struct poptOption rpmSignPoptTable[] = {
#ifdef	DYING
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
	signArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif
 { "addsign", '\0', POPT_ARG_VAL, &rpmKArgs.addSign, RPMSIGN_ADD_SIGNATURE,
	N_("add a signature to a package"), NULL },
 { "checksig", 'K', POPT_ARG_VAL, &rpmKArgs.addSign, RPMSIGN_CHK_SIGNATURE,
	N_("verify package signature"), NULL },
 { "import", 'K', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmKArgs.addSign, RPMSIGN_IMPORT_PUBKEY,
	N_("verify package signature"), NULL },
 { "resign", '\0', POPT_ARG_VAL, &rpmKArgs.addSign, RPMSIGN_NEW_SIGNATURE,
	N_("sign a package (discard current signature)"), NULL },
 { "sign", '\0', POPT_ARGFLAG_DOC_HIDDEN, &rpmKArgs.sign, 0,
	N_("generate signature"), NULL },
 { "nogpg", '\0', POPT_BIT_CLR, &rpmKArgs.checksigFlags, CHECKSIG_GPG,
	N_("skip any GPG signatures"), NULL },
 { "nopgp", '\0', POPT_BIT_CLR|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmKArgs.checksigFlags, CHECKSIG_PGP,
	N_("skip any PGP signatures"), NULL },
 { "nomd5", '\0', POPT_BIT_CLR, &rpmKArgs.checksigFlags, CHECKSIG_MD5,
	N_("do not verify file md5 checksums"), NULL },

   POPT_TABLEEND
};
