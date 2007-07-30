m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

int foo(void);

DB *job_db, *name_db, *pers_db;
DB_TXN *txn;

int
main() {
	foo();
	return (0);
}

int
foo()
{])
m4_indent([dnl
DBC *name_curs, *job_curs, *join_curs;
DBC *carray__LB__3__RB__;
DBT key, data;
int ret, tret;
m4_blank
name_curs = NULL;
job_curs = NULL;
memset(&key, 0, sizeof(key));
memset(&data, 0, sizeof(data));
m4_blank
if ((ret =
    name_db-__GT__cursor(name_db, txn, &name_curs, 0)) != 0)
	goto err;
key.data = "smith";
key.size = sizeof("smith");
if ((ret =
    name_curs-__GT__c_get(name_curs, &key, &data, DB_SET)) != 0)
	goto err;
m4_blank
if ((ret = job_db-__GT__cursor(job_db, txn, &job_curs, 0)) != 0)
	goto err;
key.data = "manager";
key.size = sizeof("manager");
if ((ret =
    job_curs-__GT__c_get(job_curs, &key, &data, DB_SET)) != 0)
	goto err;
m4_blank
carray__LB__0__RB__ = name_curs;
carray__LB__1__RB__ = job_curs;
carray__LB__2__RB__ = NULL;
m4_blank
if ((ret =
    pers_db-__GT__join(pers_db, carray, &join_curs, 0)) != 0)
	goto err;
while ((ret =
    join_curs-__GT__c_get(join_curs, &key, &data, 0)) == 0) {
	/* Process record returned in key/data. */
}
m4_blank
/*
 * If we exited the loop because we ran out of records,
 * then it has completed successfully.
 */
if (ret == DB_NOTFOUND)
	ret = 0;
m4_blank
err:
if (join_curs != NULL &&
    (tret = join_curs-__GT__c_close(join_curs)) != 0 && ret == 0)
	ret = tret;
if (name_curs != NULL &&
    (tret = name_curs-__GT__c_close(name_curs)) != 0 && ret == 0)
	ret = tret;
if (job_curs != NULL &&
    (tret = job_curs-__GT__c_close(job_curs)) != 0 && ret == 0)
	ret = tret;
m4_blank
return (ret);
])
m4_ignore([dnl
}])
