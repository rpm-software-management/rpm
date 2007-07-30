m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

u_int32_t compare_prefix(DB *, const DBT *a, const DBT *b);

int
main()
{
	DB *dbp;
	DBT key, data;

	(void)compare_prefix(dbp, &key, &data);
	return (0);
}])

m4_indent([dnl
u_int32_t
compare_prefix(dbp, a, b)
	DB *dbp;
	const DBT *a, *b;
{
	size_t cnt, len;
	u_int8_t *p1, *p2;
m4_blank
	cnt = 1;
	len = a-__GT__size __GT__ b-__GT__size ? b-__GT__size : a-__GT__size;
	for (p1 =
		a-__GT__data, p2 = b-__GT__data; len--; ++p1, ++p2, ++cnt)
			if (*p1 != *p2)
				return (cnt);

	/*
	 * They match up to the smaller of the two sizes.
	 * Collate the longer after the shorter.
	 */
	if (a-__GT__size __LT__ b-__GT__size)
		return (a-__GT__size + 1);
	if (b-__GT__size __LT__ a-__GT__size)
		return (b-__GT__size + 1);
	return (b-__GT__size);
}])
