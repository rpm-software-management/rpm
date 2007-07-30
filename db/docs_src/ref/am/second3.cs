m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

void	foo();
void	handle_error(int);

int ret;
DB *dbp;
DB_TXN *txn;

int
main()
{
	foo();
	return (0);
}

void
handle_error(ret)
	int ret;
{}

void
foo()
{])

m4_indent([dnl
DBT key;
m4_blank
memset(&key, 0, sizeof(DBT));
key.data = "WC42";
key.size = 4;
if ((ret = dbp-__GT__del(dbp, txn, &key, 0)) != 0)
	handle_error(ret);])
m4_ignore([}])
