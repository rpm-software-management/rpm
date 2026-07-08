#ifndef _H_ARGV_
#define	_H_ARGV_

/** \ingroup rpmargv
 * \file argv.h
 *
 * Argument Manipulation API.
 */

#include <stdio.h>
#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char ** ARGV_t;
typedef char * const *ARGV_const_t;

typedef	int * ARGint_t;
struct ARGI_s {
    unsigned nvals;
    ARGint_t vals;
};
typedef	struct ARGI_s * ARGI_t;
typedef	struct ARGI_s const * const ARGI_const_t;

/** \ingroup rpmargv
 * Print argv array elements.
 * @param msg		output message prefix (or NULL)
 * @param argv		argv array
 * @param fp		output file handle (NULL uses stderr)
 */
RPM_PUBLIC_API
void argvPrint(const char * msg, ARGV_const_t argv, FILE * fp);

/** \ingroup rpmargv
 * Destroy an argi array.
 * @param argi		argi array
 * @return		NULL always
 */
RPM_PUBLIC_API
ARGI_t argiFree(ARGI_t argi);


/** \ingroup rpmargv
 * Create an empty argv array.
 * @return		pointer to empty argv
 */
RPM_PUBLIC_API
ARGV_t argvNew(void);

/** \ingroup rpmargv
 * Destroy an argv array.
 * @param argv		argv array
 * @return		NULL always
 */
RPM_PUBLIC_API
ARGV_t argvFree(ARGV_t argv);

/** \ingroup rpmargv
 * Return no. of elements in argi array.
 * @param argi		argi array
 * @return		no. of elements
 */
RPM_PUBLIC_API
int argiCount(ARGI_const_t argi);

/** \ingroup rpmargv
 * Return data from argi array.
 * @param argi		argi array
 * @return		argi array data address
 */
RPM_PUBLIC_API
ARGint_t argiData(ARGI_const_t argi);

/** \ingroup rpmargv
 * Return no. of elements in argv array.
 * @param argv		argv array
 * @return		no. of elements
 */
RPM_PUBLIC_API
int argvCount(ARGV_const_t argv);

/** \ingroup rpmargv
 * Return data from argv array.
 * @param argv		argv array
 * @return		argv array data address
 */
RPM_PUBLIC_API
ARGV_t argvData(ARGV_t argv);

/** \ingroup rpmargv
 * Compare argv arrays (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
RPM_PUBLIC_API
int argvCmp(const void * a, const void * b);

/** \ingroup rpmargv
 * Sort an argv array.
 * @param argv		argv array
 * @param compar	strcmp-like comparison function, or NULL for argvCmp()
 * @return		0 always
 */
RPM_PUBLIC_API
int argvSort(ARGV_t argv, int (*compar)(const void *, const void *));

/** \ingroup rpmargv
 * Find an element in an argv array.
 * @param argv		argv array
 * @param val		string to find
 * @param compar	strcmp-like comparison function, or NULL for argvCmp()
 * @return		found string (NULL on failure)
 */
RPM_PUBLIC_API
ARGV_t argvSearch(ARGV_const_t argv, const char *val,
		int (*compar)(const void *, const void *));

/** \ingroup rpmargv
 * Add an int to an argi array.
 * @param[out] *argip	argi array
 * @param ix		argi array index (or -1 to append)
 * @param val		int arg to add
 * @return		0 always
 */
RPM_PUBLIC_API
int argiAdd(ARGI_t * argip, int ix, int val);

/** \ingroup rpmargv
 * Add a string to an argv array.
 * @param[out] *argvp	argv array
 * @param val		string arg to append
 * @return		0 always
 */
RPM_PUBLIC_API
int argvAdd(ARGV_t * argvp, const char *val);

/** \ingroup rpmargv
 * Add a string to an argv array, does not need to be nil-terminated.
 * @param[out] *argvp	argv array
 * @param val		string arg to append
 * @param len		string arg length
 * @return		0 always
 */
RPM_PUBLIC_API
int argvAddN(ARGV_t * argvp, const char *val, size_t len);

/** \ingroup rpmargv
 * Add a number to an argv array (converting to a string).
 * @param[out] *argvp	argv array
 * @param val		numeric arg to append
 * @return		0 always
 */
RPM_PUBLIC_API
int argvAddNum(ARGV_t * argvp, int val);

/** \ingroup rpmargv
 * Append one argv array to another.
 * @param[out] *argvp	argv array
 * @param av		argv array to append
 * @return		0 always
 */
RPM_PUBLIC_API
int argvAppend(ARGV_t * argvp, ARGV_const_t av);

enum argvFlags_e {
    ARGV_NONE		= 0,
    ARGV_SKIPEMPTY	= (1 << 0),	/* omit empty strings from result */
};

typedef rpmFlags argvFlags;

/** \ingroup rpmargv
 * Split a string into an argv array.
 * @param str		string arg to split
 * @param seps		separator characters
 * @param flags		flags to control behavior
 * @return		argv array
 */
RPM_PUBLIC_API
ARGV_t argvSplitString(const char * str, const char * seps, argvFlags flags);

/** \ingroup rpmargv
 * Split a string into an argv array.
 * @param[out] *argvp	argv array
 * @param str		string arg to split
 * @param seps		separator characters
 * @return		0 always
 */
RPM_PUBLIC_API
int argvSplit(ARGV_t * argvp, const char * str, const char * seps);

/** \ingroup rpmargv
 * Join an argv array into a string.
 * @param *argv		argv array to join
 * @param sep		separator string to use
 * @return		malloc'ed string
 */
RPM_PUBLIC_API
char *argvJoin(ARGV_const_t argv, const char *sep);

#ifdef __cplusplus
}
#endif

#endif /* _H_ARGV_ */
