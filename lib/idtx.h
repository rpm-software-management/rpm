#ifndef H_RPMIDTX
#define H_RPMIDTX

#include "system.h"
#include "rpmlib.h"

/**
 *  * A rollback transaction id element.
 *   */
/*@-fielduse@*/
typedef /*@abstract@*/ struct IDT_s {
    unsigned int instance;      /*!< installed package transaction id. */
/*@owned@*/ /*@null@*/
    const char * key;           /*! removed package file name. */
    Header h;                   /*!< removed package header. */
    union {
        uint_32 u32;            /*!< install/remove transaction id */
    } val;
} * IDT;
/*@=fielduse@*/

/**
 * A rollback transaction id index.
 */
typedef /*@abstract@*/ struct IDTindex_s {
    int delta;			/*!< no. elements to realloc as a chunk. */
    int size;			/*!< size of id index element. */
    int alloced;		/*!< current number of elements allocated. */
    int nidt;			/*!< current number of elements initialized. */
/*@only@*/ /*@null@*/
    IDT idt;			/*!< id index elements. */
} * IDTX;

/**
 * Destroy id index.
 * @param idtx		id index
 * @return		NULL always
 */
/*@null@*/
IDTX IDTXfree(/*@only@*/ /*@null@*/ IDTX idtx)
	/*@modifies idtx @*/;

/**
 * Create id index.
 * @return		new id index
 */
/*@only@*/
IDTX IDTXnew(void)
	/*@*/;

/**
 * Insure that index has room for "need" elements.
 * @param idtx		id index
 * @param need		additional no. of elements needed
 * @return 		id index (with room for "need" elements)
 */
/*@only@*/ /*@null@*/
IDTX IDTXgrow(/*@only@*/ /*@null@*/ IDTX idtx, int need)
	/*@modifies idtx @*/;

/**
 * Sort tag (instance,value) pairs.
 * @param idtx		id index
 * @return 		id index
 */
/*@only@*/ /*@null@*/
IDTX IDTXsort(/*@only@*/ /*@null@*/ IDTX idtx)
	/*@modifies idtx @*/;

/**
 * Load tag (instance,value) pairs from rpm databse, and return sorted id index.
 * @param ts		transaction set
 * @param tag		rpm tag
 * @return 		id index
 */
/*@only@*/ /*@null@*/
IDTX IDTXload(rpmts ts, rpmTag tag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState  @*/;

/**
 * Load tag (instance,value) pairs from packages, and return sorted id index.
 * @param ts		transaction set
 * @param globstr	glob expression
 * @param tag		rpm tag
 * @return 		id index
 */
/*@only@*/ /*@null@*/
IDTX IDTXglob(rpmts ts, const char * globstr, rpmTag tag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

#endif /* H_RPMIDTX */
