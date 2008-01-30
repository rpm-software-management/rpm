#ifndef H_RPMIDTX
#define H_RPMIDTX

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  * A rollback transaction id element.
 *   */
typedef struct IDT_s {
    unsigned int instance;      /*!< installed package transaction id. */
    const char * key;           /*! removed package file name. */
    Header h;                   /*!< removed package header. */
    union {
        uint32_t u32;            /*!< install/remove transaction id */
    } val;
} * IDT;

/**
 * A rollback transaction id index.
 */
typedef struct IDTindex_s {
    int delta;			/*!< no. elements to realloc as a chunk. */
    int size;			/*!< size of id index element. */
    int alloced;		/*!< current number of elements allocated. */
    int nidt;			/*!< current number of elements initialized. */
    IDT idt;			/*!< id index elements. */
} * IDTX;

/**
 * Destroy id index.
 * @param idtx		id index
 * @return		NULL always
 */
IDTX IDTXfree(IDTX idtx);

/**
 * Create id index.
 * @return		new id index
 */
IDTX IDTXnew(void);

/**
 * Insure that index has room for "need" elements.
 * @param idtx		id index
 * @param need		additional no. of elements needed
 * @return 		id index (with room for "need" elements)
 */
IDTX IDTXgrow(IDTX idtx, int need);

/**
 * Sort tag (instance,value) pairs.
 * @param idtx		id index
 * @return 		id index
 */
IDTX IDTXsort(IDTX idtx);

/**
 * Load tag (instance,value) pairs from rpm databse, and return sorted id index.
 * @param ts		transaction set
 * @param tag		rpm tag
 * @return 		id index
 */
IDTX IDTXload(rpmts ts, rpm_tag_t tag);

/**
 * Load tag (instance,value) pairs from packages, and return sorted id index.
 * @param ts		transaction set
 * @param globstr	glob expression
 * @param tag		rpm tag
 * @return 		id index
 */
IDTX IDTXglob(rpmts ts, const char * globstr, rpm_tag_t tag);

#ifdef __cplusplus
}   
#endif

#endif /* H_RPMIDTX */
