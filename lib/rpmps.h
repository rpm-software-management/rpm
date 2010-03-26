#ifndef H_RPMPS
#define H_RPMPS

/** \ingroup rpmps
 * \file lib/rpmps.h
 * Structures and prototypes used for an "rpmps" problem set.
 */

#include <stdio.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmprob.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmps
 * Problem set iterator
 */
typedef struct rpmpsi_s * rpmpsi;

/** \ingroup rpmps
 * Unreference a problem set instance.
 * @param ps		problem set
 * @return		problem set
 */
rpmps rpmpsUnlink (rpmps ps);

/** \ingroup rpmps
 * Reference a problem set instance.
 * @param ps		transaction set
 * @return		new transaction set reference
 */
rpmps rpmpsLink (rpmps ps);

/** \ingroup rpmps
 * Return number of problems in set.
 * @param ps		problem set
 * @return		number of problems
 */
int rpmpsNumProblems(rpmps ps);

/** \ingroup rpmps
 * Initialize problem set iterator.
 * @param ps		problem set
 * @return		problem set iterator
 */
rpmpsi rpmpsInitIterator(rpmps ps);

/** \ingroup rpmps
 * Destroy problem set iterator.
 * @param psi		problem set iterator
 * @return		problem set iterator (NULL)
 */
rpmpsi rpmpsFreeIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return next problem from iterator
 * @param psi		problem set iterator
 * @return		next problem (weak ref), NULL on termination
 */
rpmProblem rpmpsiNext(rpmpsi psi);

/** \ingroup rpmps
 * Return next problem set iterator index
 * @param psi		problem set iterator
 * @return		iterator index, -1 on termination
 */
int rpmpsNextIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return current problem from problem set
 * @param psi		problem set iterator
 * @return		current rpmProblem 
 */
rpmProblem rpmpsGetProblem(rpmpsi psi);

/** \ingroup rpmps
 * Create a problem set.
 * @return		new problem set
 */
rpmps rpmpsCreate(void);

/** \ingroup rpmps
 * Destroy a problem set.
 * @param ps		problem set
 * @return		NULL always
 */
rpmps rpmpsFree(rpmps ps);

/** \ingroup rpmps
 * Print problems to file handle.
 * @param fp		file handle (NULL uses stderr)
 * @param ps		problem set
 */
void rpmpsPrint(FILE *fp, rpmps ps);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param prob		rpmProblem 
 */
void rpmpsAppendProblem(rpmps ps, rpmProblem prob);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param number	generic number attribute
 */
void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR,
		fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR,
		uint64_t number);

/** \ingroup rpmps
 * Filter a problem set.
 *
 * As the problem sets are generated in an order solely dependent
 * on the ordering of the packages in the transaction, and that
 * ordering can't be changed, the problem sets must be parallel to
 * one another. Additionally, the filter set must be a subset of the
 * target set, given the operations available on transaction set.
 * This is good, as it lets us perform this trim in linear time, rather
 * then logarithmic or quadratic.
 *
 * @param ps		problem set
 * @param filter	problem filter (or NULL)
 * @return		0 no problems, 1 if problems remain
 */
int rpmpsTrim(rpmps ps, rpmps filter);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMPS */
