#ifndef _H_ARGV_
#define	_H_ARGV_

/** \ingroup rpmbuild
 * \file build/argv.h
 */

typedef	const char * ARG_t;
typedef ARG_t * ARGV_t;

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
 * Destroy an argv array.
 * @param argv		argv array
 */
int argvFree(/*@only@*/ /*@null@*/ ARGV_t argv)
	/*@modifies argv @*/;

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
ARGV_t argvSearch(ARGV_t argv, ARG_t s,
		int (*compar)(const void *, const void *))
	/*@*/;

/**
 * Append one argv array to another.
 * @retval *argvp	argv array
 * @param av		argv array to append
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
