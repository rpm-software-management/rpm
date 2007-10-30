#ifndef H_RPMTSSCORE
#define H_RPMTSSCORE

#include "rpmlib.h"
#include "rpmte.h"

/**********************
 * Transaction Scores *
 **********************
 *
 * In order to allow instance counts to be adjusted properly when an
 * autorollback transaction is ran, we keep a list that is indexed
 * by rpm name of whether the rpm has been installed or erased.  This listed
 * is only updated:
 *
 *	iif autorollbacks are enabled.
 *	iif this is not a rollback or autorollback transaction.
 *
 * When creating an autorollback transaction, its rpmts points to the same
 * rpmtsScore object as the running transaction.  So when the autorollback
 * transaction runs it can see where each package was in the running transaction
 * at the point the running transaction failed, and thus on a per package
 * basis make adjustments to the instance counts.
 *
 */

struct rpmtsScoreEntry_s {
    char *         N;                   /*!<Name of package                */
    rpmElementType te_types;            /*!<te types this entry represents */
    int            installed;           /*!<Was the new header installed   */
    int            erased;              /*!<Was the old header removed     */
};

typedef struct rpmtsScoreEntry_s * rpmtsScoreEntry;

struct rpmtsScore_s {
        int entries;                    /*!< Number of scores       */
        rpmtsScoreEntry * scores;       /*!< Array of score entries */
        int nrefs;                      /*!< Reference count.       */
};

typedef struct rpmtsScore_s * rpmtsScore;

/** \ingroup rpmts
 * initialize rpmtsScore for running transaction and autorollback
 * transaction.
 * @param runningTS	Running Transaction.
 * @param rollbackTS	Rollback Transaction.
 * @return		RPMRC_OK
 */
rpmRC rpmtsScoreInit(rpmts runningTS, rpmts rollbackTS);

/** \ingroup rpmts
 * Free rpmtsScore provided no more references exist against it.
 * @param score		rpmtsScore to free
 * @return		NULL always
 */
rpmtsScore rpmtsScoreFree(rpmtsScore score);

/** \ingroup rpmts
 * Get rpmtsScore from transaction.
 * @param ts	RPM Transaction.
 * @return	rpmtsScore or NULL.
 */
rpmtsScore rpmtsGetScore(rpmts ts);

/** \ingroup rpmts
 * Get rpmtsScoreEntry from rpmtsScore.
 * @param score   RPM Transaction Score.
 * @return	  rpmtsScoreEntry or NULL.
 */
rpmtsScoreEntry rpmtsScoreGetEntry(rpmtsScore score, const char *N);

#endif /* H_RPMTSSCORE */
