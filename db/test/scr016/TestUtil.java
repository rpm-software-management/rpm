/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestUtil.java,v 1.4 2003/09/04 23:41:21 bostic Exp $
 */

/*
 * Utilities used by many tests.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestUtil
{
    public static void populate(Db db)
        throws DbException
    {
        // populate our massive database.
        Dbt keydbt = new Dbt("key".getBytes());
        Dbt datadbt = new Dbt("data".getBytes());
        db.put(null, keydbt, datadbt, 0);

        // Now, retrieve.  We could use keydbt over again,
        // but that wouldn't be typical in an application.
        Dbt goodkeydbt = new Dbt("key".getBytes());
        Dbt badkeydbt = new Dbt("badkey".getBytes());
        Dbt resultdbt = new Dbt();
        resultdbt.setFlags(Db.DB_DBT_MALLOC);

        int ret;

        if ((ret = db.get(null, goodkeydbt, resultdbt, 0)) != 0) {
            System.out.println("get: " + DbEnv.strerror(ret));
        }
        else {
            String result =
                new String(resultdbt.getData(), 0, resultdbt.getSize());
            System.out.println("got data: " + result);
        }

        if ((ret = db.get(null, badkeydbt, resultdbt, 0)) != 0) {
            // We expect this...
            System.out.println("get using bad key: " + DbEnv.strerror(ret));
        }
        else {
            String result =
                new String(resultdbt.getData(), 0, resultdbt.getSize());
            System.out.println("*** got data using bad key!!: " + result);
        }
    }
}
