#ifndef H_DB185
#define H_DB185

/** \file lib/db1.h
 * Access RPM indices using Berkeley db2 with db-1.85 API.
 */

#define	DB_VERSION_MAJOR	1
#define	DB_VERSION_MINOR	85
#define	DB_VERSION_PATCH	0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return handle for an index database.
 * @param dbi	index database handle
 * @return	0 success 1 fail
 */
int db1open(dbiIndex dbi);

/**
 * Close index database.
 * @param dbi	index database handle
 * @param flags
 */
int db1close(dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi	index database handle
 * @param flags
 */
int db1sync(dbiIndex dbi, unsigned int flags);

/**
 * Return first index database key.
 * @param dbi	index database handle
 * @param key	address of first key
 * @return	0 success - fails if rec is not found
 */
int db1GetFirstKey(dbiIndex dbi, const char ** keyp);

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
int db1SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set);

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param str	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
int db1UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set);

#ifdef __cplusplus
}
#endif

#endif  /* H_DB185 */

