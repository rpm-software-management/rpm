/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: TestLogc.cpp,v 1.3 2001/10/12 13:02:31 dda Exp 
 */

/*
 * A basic regression test for the Logc class.
 */

#include <db_cxx.h>
#include <iostream.h>

static void show_dbt(ostream &os, Dbt *dbt)
{
	int i;
	int size = dbt->get_size();
	unsigned char *data = (unsigned char *)dbt->get_data();

	os << "size: " << size << " data: ";
	for (i=0; i<size && i<10; i++) {
		os << (int)data[i] << " ";
	}
	if (i<size)
		os << "...";
}

int main(int argc, char *argv[])
{
	try {
		DbEnv *env = new DbEnv(0);
		env->open(".", DB_CREATE | DB_INIT_LOG | DB_INIT_MPOOL, 0);

		// Do some database activity to get something into the log.
		Db *db1 = new Db(env, 0);
		db1->open("first.db", NULL, DB_BTREE, DB_CREATE, 0);
		Dbt *key = new Dbt((char *)"a", 1);
		Dbt *data = new Dbt((char *)"b", 1);
		db1->put(NULL, key, data, 0);
		key->set_data((char *)"c");
		data->set_data((char *)"d");
		db1->put(NULL, key, data, 0);
		db1->close(0);

		Db *db2 = new Db(env, 0);
		db2->open("second.db", NULL, DB_BTREE, DB_CREATE, 0);
		key->set_data((char *)"w");
		data->set_data((char *)"x");
		db2->put(NULL, key, data, 0);
		key->set_data((char *)"y");
		data->set_data((char *)"z");
		db2->put(NULL, key, data, 0);
		db2->close(0);

		// Now get a log cursor and walk through.
		DbLogc *logc;

		env->log_cursor(&logc, 0);
		int ret = 0;
		DbLsn lsn;
		Dbt *dbt = new Dbt();
		u_int32_t flags = DB_FIRST;

		int count = 0;
		while ((ret = logc->get(&lsn, dbt, flags)) == 0) {
			cout << "logc.get: " << count;

			// We ignore the contents of the log record,
			// it's not portable.
			//
			//     show_dbt(cout, dbt);
			//

			cout << "\n";
			count++;
			flags = DB_NEXT;
		}
		if (ret != DB_NOTFOUND) {
			cerr << "*** Failed to get log record, returned: "
			     << DbEnv::strerror(ret) << "\n";
		}
		logc->close(0);
		cout << "TestLogc done.\n";
	}
	catch (DbException &dbe) {
		cerr << "Db Exception: " << dbe.what();
	}
	return 0;
}
