/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: TestLogc.java,v 1.4 2001/10/12 13:02:33 dda Exp 
 */

/*
 * A basic regression test for the Logc class.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestLogc
{
    public static void main(String[] args)
    {
        try {
            DbEnv env = new DbEnv(0);
            env.open(".", Db.DB_CREATE | Db.DB_INIT_LOG | Db.DB_INIT_MPOOL, 0);

            // Do some database activity to get something into the log.
            Db db1 = new Db(env, 0);
            db1.open("first.db", null, Db.DB_BTREE, Db.DB_CREATE, 0);
            db1.put(null, new Dbt("a".getBytes()), new Dbt("b".getBytes()), 0);
            db1.put(null, new Dbt("c".getBytes()), new Dbt("d".getBytes()), 0);
            db1.close(0);

            Db db2 = new Db(env, 0);
            db2.open("second.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);
            db2.put(null, new Dbt("w".getBytes()), new Dbt("x".getBytes()), 0);
            db2.put(null, new Dbt("y".getBytes()), new Dbt("z".getBytes()), 0);
            db2.close(0);

            // Now get a log cursor and walk through.
            DbLogc logc = env.log_cursor(0);

            int ret = 0;
            DbLsn lsn = new DbLsn();
            Dbt dbt = new Dbt();
            int flags = Db.DB_FIRST;

            int count = 0;
            while ((ret = logc.get(lsn, dbt, flags)) == 0) {
                System.out.println("logc.get: " + count);

                // We ignore the contents of the log record,
                // it's not portable.
                //
                //     System.out.println(showDbt(dbt));
                //
                count++;
                flags = Db.DB_NEXT;
            }
            if (ret != Db.DB_NOTFOUND) {
                System.err.println("*** Failed to get log record, returned: " +
                                   DbEnv.strerror(ret));
            }
            logc.close(0);
            System.out.println("TestLogc done.");
        }
        catch (DbException dbe) {
            System.err.println("Db Exception: " + dbe);
        }
        catch (FileNotFoundException fnfe) {
            System.err.println("FileNotFoundException: " + fnfe);
        }

    }

    public static String showDbt(Dbt dbt)
    {
        StringBuffer sb = new StringBuffer();
        int size = dbt.get_size();
        byte[] data = dbt.get_data();
        int i;
        for (i=0; i<size && i<10; i++) {
            sb.append(Byte.toString(data[i]));
            sb.append(' ');
        }
        if (i<size)
            sb.append("...");
        return "size: " + size + " data: " + sb.toString();
    }
}
