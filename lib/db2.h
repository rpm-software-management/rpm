#ifndef H_DB2
#define H_DB2

/** \file lib/db2.h
 * Access RPM indices using Berkeley db-2.x API.
 */

#define	RET_ERROR	1
#define	RET_SUCCESS	0
#define	RET_SPECIAL	-1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return handle for an index database.
 * @param dbi	index database handle
 * @return	0 success 1 fail
 */
int db2open(dbiIndex dbi);

/**
 * Close index database.
 * @param dbi	index database handle
 * @param flags
 */
int db2close(dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi	index database handle
 * @param flags
 */
int db2sync(dbiIndex dbi, unsigned int flags);

/**
 * Return first index database key.
 * @param dbi	index database handle
 * @param key	address of first key
 * @return	0 success - fails if rec is not found
 */
int db2GetFirstKey(dbiIndex dbi, const char ** keyp);

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
int db2SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set);

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param str	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
int db2UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set);

#ifdef __cplusplus
}
#endif

#endif  /* H_DB2 */

