#ifndef H_DB3
#define H_DB3

/** \file lib/db3.h
 * Access RPM indices using Berkeley db-3.x API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return handle for an index database.
 * @param dbi	index database handle
 * @return	0 success 1 fail
 */
int db3open(dbiIndex dbi);

/**
 * Close index database.
 * @param dbi	index database handle
 * @param flags
 */
int db3close(dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi	index database handle
 * @param flags
 */
int db3sync(dbiIndex dbi, unsigned int flags);

/**
 * Return first index database key.
 * @param dbi	index database handle
 * @param key	address of first key
 * @return	0 success - fails if rec is not found
 */
int db3GetFirstKey(dbiIndex dbi, const char ** keyp);

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
int db3SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set);

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param str	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
int db3UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set);

#ifdef __cplusplus
}
#endif

#endif  /* H_DB3 */

