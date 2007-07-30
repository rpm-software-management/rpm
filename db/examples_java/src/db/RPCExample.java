/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: RPCExample.java,v 12.6 2007/05/17 15:15:36 bostic Exp $
 */

package db;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.OutputStream;

/*
 * An example of a program configuring a database environment as an RPC client.
 */
public class RPCExample {
    private static final String progname = "RPCExample";
    private static final File DATABASE_HOME = new File("TESTDIR");

    private static void runApplication(Environment dbenv)
        throws DatabaseException, FileNotFoundException {

        // Do something interesting...
        // Your application goes here.
	DatabaseConfig config = new DatabaseConfig();
	config.setAllowCreate(true);
	config.setType(DatabaseType.BTREE);
        Database db = dbenv.openDatabase(null, "test.db", null, config);
	db.close();
    }

    private static void setupEnvironment(File home,
                                         OutputStream errs)
        throws DatabaseException, FileNotFoundException {

        // Create an environment object and initialize it for error reporting.
        EnvironmentConfig config = new EnvironmentConfig();
        config.setErrorStream(errs);
        config.setErrorPrefix(progname);

        //
        // We want to specify the shared memory buffer pool cachesize,
        // but everything else is the default.
        //
        config.setCacheSize(64 * 1024);

        // Open the environment with full transactional support.
        config.setAllowCreate(true);
        config.setInitializeCache(true);
        config.setTransactional(true);
        config.setInitializeLocking(true);

	config.setRPCServer("localhost", 0, 0);

        //
        // open is declared to throw a FileNotFoundException, which normally
        // shouldn't occur when allowCreate is set.
        //
        Environment dbenv = new Environment(home, config);

        try {
            // Start your application.
            runApplication(dbenv);
        } finally {
            // Close the environment.  Doing this in the finally block ensures
            // it is done, even if an error is thrown.
            dbenv.close();
        }
    }

    private static void teardownEnvironment(File home,
                                            OutputStream errs)
        throws DatabaseException, FileNotFoundException {

        // Remove the shared database regions.
        EnvironmentConfig config = new EnvironmentConfig();

        config.setErrorStream(errs);
        config.setErrorPrefix(progname);
	config.setRPCServer("localhost", 0, 0);
        Environment.remove(home, true, config);
    }

    public static void main(String[] args) {
        File home = DATABASE_HOME;

        try {
            System.out.println("Setup env");
            setupEnvironment(home, System.err);

            System.out.println("Teardown env");
            teardownEnvironment(home, System.err);
        } catch (DatabaseException dbe) {
            System.err.println(progname + ": environment open: " + dbe.toString());
	    dbe.printStackTrace(System.err);
            System.exit (1);
        } catch (FileNotFoundException fnfe) {
            System.err.println(progname + ": unexpected open environment error  " + fnfe);
            System.exit (1);
        }
    }

}
