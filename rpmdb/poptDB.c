/** \ingroup rpmcli
 * \file rpmdb/poptDB.c
 *  Popt tables for database modes.
 */

#include "system.h"

#include <rpmcli.h>

#include "debug.h"

struct rpmDatabaseArguments_s rpmDBArgs;

/**
 */
struct poptOption rpmDatabasePoptTable[] = {
 { "initdb", '\0', POPT_ARG_VAL, &rpmDBArgs.init, 1,
	N_("initialize database"), NULL},
 { "rebuilddb", '\0', POPT_ARG_VAL, &rpmDBArgs.rebuild, 1,
	N_("rebuild database inverted lists from installed package headers"),
	NULL},
 { "verifydb", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &rpmDBArgs.verify, 1,
	N_("verify database files"), NULL},

   POPT_TABLEEND
};
