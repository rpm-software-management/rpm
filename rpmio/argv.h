#ifndef _H_ARGV_
#define	_H_ARGV_

/** \ingroup rpmio
 * \file rpmio/argv.h
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef	const char * ARGstr_t;
typedef ARGstr_t * ARGV_t;

typedef	int * ARGint_t;
struct ARGI_s {
    unsigned nvals;
    ARGint_t vals;
};
typedef	struct ARGI_s * ARGI_t;

/**
 * Print argv array elements.
 * @param msg		output message prefix (or NULL)
 * @param argv		argv array
 * @param fp		output file handle (NULL uses stderr)
 */
void argvPrint(const char * msg, ARGV_t argv, FILE * fp);

/**
 * Destroy an argi array.
 * @param argi		argi array
 * @return		NULL always
 */
ARGI_t argiFree(ARGI_t argi);

/**
 * Destroy an argv array.
 * @param argv		argv array
 * @return		NULL always
 */
ARGV_t argvFree(ARGV_t argv);

/**
 * Return no. of elements in argi array.
 * @param argi		argi array
 * @return		no. of elements
 */
int argiCount(const ARGI_t argi);

/**
 * Return data from argi array.
 * @param argi		argi array
 * @return		argi array data address
 */
ARGint_t argiData(const ARGI_t argi);

/**
 * Return no. of elements in argv array.
 * @param argv		argv array
 * @return		no. of elements
 */
int argvCount(const ARGV_t argv);

/**
 * Return data from argv array.
 * @param argv		argv array
 * @return		argv array data address
 */
ARGV_t argvData(const ARGV_t argv);

/**
 * Compare argv arrays (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
int argvCmp(const void * a, const void * b);

/**
 * Sort an argv array.
 * @param argv		argv array
 * @param compar	strcmp-like comparison function, or NULL for argvCmp()
 * @return		0 always
 */
int argvSort(ARGV_t argv, int (*compar)(const void *, const void *));

/**
 * Find an element in an argv array.
 * @param argv		argv array
 * @param val		string to find
 * @param compar	strcmp-like comparison function, or NULL for argvCmp()
 * @return		found string (NULL on failure)
 */
ARGV_t argvSearch(ARGV_t argv, ARGstr_t val,
		int (*compar)(const void *, const void *));

/**
 * Add an int to an argi array.
 * @retval *argip	argi array
 * @param ix		argi array index (or -1 to append)
 * @param val		int arg to add
 * @return		0 always
 */
int argiAdd(ARGI_t * argip, int ix, int val);

/**
 * Add a string to an argv array.
 * @retval *argvp	argv array
 * @param val		string arg to append
 * @return		0 always
 */
int argvAdd(ARGV_t * argvp, ARGstr_t val);

/**
 * Append one argv array to another.
 * @retval *argvp	argv array
 * @param av		argv array to append
 * @return		0 always
 */
int argvAppend(ARGV_t * argvp, const ARGV_t av);

/**
 * Split a string into an argv array.
 * @retval *argvp	argv array
 * @param str		string arg to split
 * @param seps		seperator characters
 * @return		0 always
 */
int argvSplit(ARGV_t * argvp, const char * str, const char * seps);

#ifdef __cplusplus
}
#endif

#endif /* _H_ARGV_ */
