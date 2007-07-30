m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

int compare_int(DB *, const DBT *a, const DBT *b);

int
main()
{
	DB *dbp;
	DBT key, data;

	(void)compare_int(dbp, &key, &data);
	return (0);
}])
m4_indent([dnl
int
compare_int(dbp, a, b)
	DB *dbp;
	const DBT *a, *b;
{
	int ai, bi;
m4_blank
	/*
	 * Returns:
	 *	__LT__ 0 if a __LT__ b
	 *	= 0 if a = b
	 *	__GT__ 0 if a __GT__ b
	 */
	memcpy(&ai, a-__GT__data, sizeof(int));
	memcpy(&bi, b-__GT__data, sizeof(int));
	return (ai - bi);
}])
