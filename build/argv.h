#ifndef _H_ARGV_
#define	_H_ARGV_

/** \ingroup rpmbuild
 * \file build/argv.h
 */

typedef	const char * ARGstr_t;
typedef ARGstr_t * ARGV_t;

typedef	int * ARGint_t;
struct ARGI_s {
    unsigned nvals;
    ARGint_t vals;
};
typedef	struct ARGI_s * ARGI_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print argv array elements.
 * @param msg		output message prefix (or NULL)
 * @param argv		argv array
 * @param fp		output file handle (NULL uses stderr)
 */
void argvPrint(const char * msg, ARGV_t argv, FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 * Destroy an argi array.
 * @param argi		argi array
 * @return		NULL always
 */
/*@null@*/
ARGI_t argiFree(/*@only@*/ /*@null@*/ ARGI_t argi)
	/*@modifies argi @*/;

/**
 * Destroy an argv array.
 * @param argv		argv array
 * @return		NULL always
 */
/*@null@*/
ARGV_t argvFree(/*@only@*/ /*@null@*/ ARGV_t argv)
	/*@modifies argv @*/;

/**
 * Return no. of elements in argi array.
 * @param argi		argi array
 */
int argiCount(/*@null@*/ const ARGI_t argi)
	/*@*/;

/**
 * Return no. of elements in argv array.
 * @param argv		argv array
 */
int argvCount(/*@null@*/ const ARGV_t argv)
	/*@*/;

/**
 * Compare argv arrays (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
int argvCmp(const void * a, const void * b)
	/*@*/;

/**
 * Sort an argv array.
 * @param argv		argv array
 */
int argvSort(ARGV_t argv, int (*compar)(const void *, const void *))
	/*@modifies *argv @*/;

/**
 * Find an element in an argv array.
 * @param argv		argv array
 */
/*@dependent@*/ /*@null@*/
ARGV_t argvSearch(ARGV_t argv, ARGstr_t val,
		int (*compar)(const void *, const void *))
	/*@*/;

/**
 * Add an int to an argi array.
 * @retval *argip	argi array
 * @parm ix		argi array index (or -1 to append)
 * @param val		int arg to add
 * @return		0 always
 */
int argiAdd(/*@out@*/ ARGI_t * argip, int ix, int val)
	/*@modifies *argip @*/;

/**
 * Add a string to an argv array.
 * @retval *argvp	argv array
 * @param val		string arg to append
 * @return		0 always
 */
int argvAdd(/*@out@*/ ARGV_t * argvp, ARGstr_t val)
	/*@modifies *argvp @*/;

/**
 * Append one argv array to another.
 * @retval *argvp	argv array
 * @param av		argv array to append
 * @return		0 always
 */
int argvAppend(/*@out@*/ ARGV_t * argvp, const ARGV_t av)
	/*@modifies *argvp @*/;

/**
 * Splint a string into an argv array.
 * @retval *argvp	argv array
 */
int argvSplit(ARGV_t * argvp, const char * str, const char * seps)
	/*@modifies *argvp @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_ARGV_ */
