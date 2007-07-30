m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

DB_ENV *dbenv;

void	second();
int	getname(DB *, const DBT *, const DBT *, DBT *);
void	handle_error(int);

int
main()
{
	second();
	return (0);
}

void
handle_error(ret)
	int ret;
{}])
m4_literal([m4_indentv([dnl
struct student_record {
	char student_id__LB__4__RB__;
	char last_name__LB__15__RB__;
	char first_name__LB__15__RB__;
};
m4_blank
void
second()
{
	DB *dbp, *sdbp;
	int ret;
	m4_blank
	/* Open/create primary */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		handle_error(ret);
	if ((ret = dbp-__GT__open(dbp, NULL,
	    "students.db", NULL, DB_BTREE, DB_CREATE, 0600)) != 0)
		handle_error(ret);
	m4_blank
	/*
	 * Open/create secondary.  Note that it supports duplicate data
	 * items, since last names might not be unique.
	 */
	if ((ret = db_create(&sdbp, dbenv, 0)) != 0)
		handle_error(ret);
	if ((ret = sdbp-__GT__set_flags(sdbp, DB_DUP | DB_DUPSORT)) != 0)
		handle_error(ret);
	if ((ret = sdbp-__GT__open(sdbp, NULL,
	    "lastname.db", NULL, DB_BTREE, DB_CREATE, 0600)) != 0)
		handle_error(ret);
	m4_blank
	/* Associate the secondary with the primary. */
	if ((ret = dbp-__GT__associate(dbp, NULL, sdbp, getname, 0)) != 0)
		handle_error(ret);
}
m4_blank
/*
 * getname -- extracts a secondary key (the last name) from a primary
 * 	key/data pair
 */
int
getname(secondary, pkey, pdata, skey)
	DB *secondary;
	const DBT *pkey, *pdata;
	DBT *skey;
{
	/*
	 * Since the secondary key is a simple structure member of the
	 * record, we don't have to do anything fancy to return it.  If
	 * we have composite keys that need to be constructed from the
	 * record, rather than simply pointing into it, then the user's
	 * function might need to allocate space and copy data.  In
	 * this case, the DB_DBT_APPMALLOC flag should be set in the
	 * secondary key DBT.
	 */
	memset(skey, 0, sizeof(DBT));
	skey-__GT__data = ((struct student_record *)pdata-__GT__data)-__GT__last_name;
	skey-__GT__size = sizeof((struct student_record *)pdata-__GT__data)-__GT__last_name;
	return (0);
}])])
