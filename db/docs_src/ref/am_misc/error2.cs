m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int foo();

int	session_id;
char   *program_name;

DB *dbp;
DBT key, data;

int
main()
{
	(void)foo();
	return (0);
}

int
foo()
{])
m4_indent([dnl
#define DATABASE "access.db"
m4_blank
int ret;
m4_blank
(void)dbp-__GT__set_errpfx(dbp, program_name);
m4_blank
if ((ret = dbp-__GT__open(dbp,
    NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
	dbp-__GT__err(dbp, ret, "%s", DATABASE);
	dbp-__GT__errx(dbp,
		"contact your system administrator: session ID was %d",
    		session_id);
	return (1);
}])
m4_ignore([dnl
	return (0);
}])
