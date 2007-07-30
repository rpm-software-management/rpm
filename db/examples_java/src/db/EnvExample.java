/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: EnvExample.java,v 12.6 2007/05/17 15:15:36 bostic Exp $
 */

package db;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.OutputStream;

/*
 * An example of a program configuring a database environment.
 *
 * For comparison purposes, this example uses a similar structure
 * as examples/ex_env.c and examples_cxx/EnvExample.cpp.
 */
public class EnvExample {
    private static final String progname = "EnvExample";
    private static final File DATABASE_HOME = new File("/tmp/database");

    private static void runApplication(Environment dbenv)
        throws DatabaseException {

        // Do something interesting...
        // Your application goes here.
    }

    private static void setupEnvironment(File home,
                                         String dataDir,
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

        // Databases are in a separate directory.
        config.addDataDir(dataDir);

        // Open the environment with full transactional support.
        config.setAllowCreate(true);
        config.setInitializeCache(true);
        config.setTransactional(true);
        config.setInitializeLocking(true);

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
                                            String dataDir,
                                            OutputStream errs)
        throws DatabaseException, FileNotFoundException {

        // Remove the shared database regions.
        EnvironmentConfig config = new EnvironmentConfig();

        config.setErrorStream(errs);
        config.setErrorPrefix(progname);
        config.addDataDir(dataDir);
        Environment.remove(home, true, config);
    }

    public static void main(String[] args) {
        //
        // All of the shared database files live in /tmp/database,
        // but data files live in /database/files.
        //
        // Using Berkeley DB in C/C++, we need to allocate two elements
        // in the array and set config[1] to NULL.  This is not
        // necessary in Java.
        //
        File home = DATABASE_HOME;
        String dataDir = "/database/files";

        try {
            System.out.println("Setup env");
            setupEnvironment(home, dataDir, System.err);

            System.out.println("Teardown env");
            teardownEnvironment(home, dataDir, System.err);
        } catch (DatabaseException dbe) {
            System.err.println(progname + ": environment open: " + dbe.toString());
            System.exit (1);
        } catch (FileNotFoundException fnfe) {
            System.err.println(progname + ": unexpected open environment error  " + fnfe);
            System.exit (1);
        }
    }

}
