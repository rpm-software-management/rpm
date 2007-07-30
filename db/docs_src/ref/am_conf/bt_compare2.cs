m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int compare_dbt(DB *, const DBT *a, const DBT *b);

int
main()
{
	DB *dbp;
	DBT key, data;

	(void)compare_dbt(dbp, &key, &data);
	return (0);
}])
m4_indent([dnl
int
compare_dbt(dbp, a, b)
	DB *dbp;
	const DBT *a, *b;
{
	int len;
	u_char *p1, *p2;
m4_blank
	/*
	 * Returns:
	 *	__LT__ 0 if a __LT__ b
	 *	= 0 if a = b
	 *	__GT__ 0 if a __GT__ b
	 */
	for (p1 = a-__GT__data, p2 = b-__GT__data, len = 5; len--; ++p1, ++p2)
		if (*p1 != *p2)
			return ((long)*p1 - (long)*p2);
	return (0);
}])
