#ifndef _RPMTD_H
#define _RPMTD_H

#include <rpm/rpmtypes.h>

/** \ingroup rpmtd
 * Container for rpm tag data (from headers or extensions).
 */
struct rpmtd_s {
    rpmTag tag;		/* rpm tag of this data entry*/
    rpmTagType type;	/* data type */
    rpm_count_t count;	/* number of entries */
    rpm_data_t data;	/* pointer to actual data */
    int freeData;	/* was data malloced */
    int ix;		/* iteration index */
};

/** \ingroup rpmtd
 * Create new tag data container
 * ®return		New, initialized tag data container.
 */
rpmtd rpmtdNew(void);

/** \ingroup rpmtd
 * Destroy tag data container.
 * @param td		Tag data container
 * @return		NULL always
 */
rpmtd rpmtdFree(rpmtd td);
 
/** \ingroup rpmtd
 * (Re-)initialize tag data container. Contents will be zeroed out
 * and iteration index reset.
 * @param td		Tag data container
 */
void rpmtdReset(rpmtd td);

/** \ingroup rpmtd
 * Free contained data. This is always safe to call as the container knows 
 * if data was malloc'ed or not. Container is reinitialized.
 * @param td		Tag data container
 */
void rpmtdFreeData(rpmtd td);

/** \ingroup rpmtd
 * Retrieve number of entries in the container.
 * @param td		Tag data container
 * @return		Number of entries in contained data.
 */
rpm_count_t rpmtdCount(rpmtd td);

/** \ingroup rpmtd
 * Retrieve tag of the container.
 * @param td		Tag data container
 * @return		Rpm tag.
 */
rpmTag rpmtdTag(rpmtd td);

/** \ingroup rpmtd
 * Initialize tag container for iteration
 * @param td		Tag data container
 * @return		0 on success
 */
int rpmtdInit(rpmtd td);

/** \ingroup rpmtd
 * Iterate over tag data container.
 * @param td		Tag data container
 * @return		Tag data container iterator index, -1 on termination
 */
int rpmtdNext(rpmtd td);

/** \ingroup rpmtd
 * Return string data from tag container.
 * For string types, just return the string. On string array types,
 * return the string from current iteration index. If the tag container
 * is not for a string type, NULL is returned.
 * @param td		Tag data container
 * @return		String constant from container, NULL on error
 */
const char * rpmtdGetString(rpmtd td);

#endif /* _RPMTD_H */
