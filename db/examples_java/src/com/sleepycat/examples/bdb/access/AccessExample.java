/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: AccessExample.java,v 1.1 2003/12/15 21:44:09 jbj Exp $
 */

package com.sleepycat.examples.bdb.access;

import java.lang.*;
import java.io.*;
import java.util.*;
import com.sleepycat.db.*;
import com.sleepycat.bdb.*;
import com.sleepycat.bdb.bind.*;
import com.sleepycat.bdb.collection.*;

/**
 *  AccesssExample mirrors the functionality of a class by the same name
 * used to demonstrate the com.sleepycat.db Java API. This version makes
 * use of the new com.sleepycat.bdb.* collections style classes to make
 * life easier.
 *
 *@author     Gregory Burd <gburd@sleepycat.com>
 *@created    October 22, 2002
 */
public class AccessExample
         implements Runnable {

    // Class Variables of AccessExample class
    private static boolean create = true;
    private static final int EXIT_SUCCESS = 0;
    private static final int EXIT_FAILURE = 1;

    public static void usage() {

	System.out.println("usage: java " +
	  "com.sleepycat.examples.bdb.access.AccessExample [-r] [database]\n");
	System.exit(EXIT_FAILURE);
    }

    /**
     *  The main program for the AccessExample class
     *
     *@param  argv  The command line arguments
     */
    public static void main(String[] argv) {

	boolean removeExistingDatabase = false;
	String databaseName = "access.db";

	for (int i = 0; i < argv.length; i++) {
	    if (argv[i].equals("-r")) {
		removeExistingDatabase = true;
	    } else if (argv[i].equals("-?")) {
		usage();
	    } else if (argv[i].startsWith("-")) {
		usage();
	    } else {
		if ((argv.length - i) != 1)
		    usage();
		databaseName = argv[i];
		break;
	    }
	}


        try {
	    // Remove the previous database.
	    if (removeExistingDatabase)
		new File(databaseName).delete();

            // don't run recovery if we just deleteeted the database if it
            // existed.
            int envFlags = Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL;
            if (create) {
                envFlags |= Db.DB_CREATE;
            }
            if (!removeExistingDatabase) {
                envFlags |= Db.DB_RECOVER;
            }
            DbEnv env = new DbEnv(0);
            env.open(".", envFlags, 0);

            // create the app and run it
            AccessExample app = new AccessExample(env, databaseName);
            app.run();
        } catch (DbException e) {
            System.err.println("AccessExample: " + e.toString());
            System.exit(1);
        } catch (FileNotFoundException e) {
            System.err.println("AccessExample: " + e.toString());
            System.exit(1);
        } catch (Exception e) {
            System.err.println("AccessExample: " + e.toString());
            System.exit(1);
        }
        System.exit(0);
    }


    private DataStore store;
    private SortedMap map;
    private DbEnv env;


    /**
     *  Constructor for the AccessExample object
     *
     *@param  env            Description of the Parameter
     *@exception  Exception  Description of the Exception
     */
    public AccessExample(DbEnv env, String databaseName)
             throws Exception {

        this.env = env;

        //
        // Lets mimic the com.sleepycat.db.AccessExample 100%
        // and use plain old byte arrays to store the key strings.
        //
        ByteArrayFormat keyFormat = new ByteArrayFormat();
        ByteArrayBinding keyBinding = new ByteArrayBinding(keyFormat);

        //
        // Lets mimic the com.sleepycat.db.AccessExample 100%
        // and use plain old byte arrays to store the value strings.
        //
        ByteArrayFormat valueFormat = new ByteArrayFormat();
        ByteArrayBinding valueBinding = new ByteArrayBinding(valueFormat);

        //
        // Open a BTREE (sorted) data store.
        //
        int dbFlags = Db.DB_AUTO_COMMIT;
        if (create) {
            dbFlags |= Db.DB_CREATE;
        }
        Db db = new Db(env, 0);
        db.open(null, databaseName, null, Db.DB_BTREE, dbFlags, 0);
        this.store = new DataStore(db, keyFormat, valueFormat, null);

        //
        // Now create a collection style map view of the data store
        // so that it is easy to work with the data in the database.
        //
        this.map = new StoredSortedMap(store, keyBinding, valueBinding, true);
    }


    /**
     *  Main processing method for the AccessExample object
     */
    public void run() {
        //
        // Insert records into a Stored Sorted Map Database, where
        // the key is the user input and the data is the user input
        // in reverse order.
        //
        final InputStreamReader reader = new InputStreamReader(System.in);

        for (; ; ) {
            final String line = askForLine(reader, System.out, "input> ");
            if (line == null) {
                break;
            }

            final String reversed =
                    (new StringBuffer(line)).reverse().toString();

            log("adding: \"" +
		line + "\" : \"" +
		reversed + "\"");

            // Do the work to add the key/value to the HashMap here.
            TransactionRunner tr = new TransactionRunner(env);
            try {
                tr.run(
                    new TransactionWorker() {
                        public void doWork() {
			    if (!map.containsKey(line.getBytes()))
				map.put(line.getBytes(), reversed.getBytes());
			    else
				System.out.println("Key " + line +
						   " already exists.");
                        }
                    });
            } catch (com.sleepycat.db.DbException e) {
                System.err.println("AccessExample: " + e.toString());
                System.exit(1);
            } catch (java.lang.Exception e) {
                System.err.println("AccessExample: " + e.toString());
                System.exit(1);
            }
        }
        System.out.println("");

        // Do the work to traverse and print the HashMap key/data
        // pairs here get iterator over map entries.
        Iterator iter = map.entrySet().iterator();
        try {
            System.out.println("Reading data");
            while (iter.hasNext()) {
                Map.Entry entry = (Map.Entry) iter.next();
                log("found \"" +
		    new String((byte[]) entry.getKey()) +
		    "\" key with value \"" +
		    new String((byte[]) entry.getValue()) + "\"");
            }
        } finally {
            // Ensure that all database iterators are closed.  This is very
            // important.
            StoredIterator.close(iter);
        }
    }


    /**
     *  Prompts for a line, and keeps prompting until a non blank line is
     *  returned. Returns null on error.
     *
     *@param  reader  stream from which to read user input
     *@param  out     stream on which to prompt for user input
     *@param  prompt  prompt to use to solicit input
     *@return         the string supplied by the user
     */
    String askForLine(InputStreamReader reader, PrintStream out,
                      String prompt) {

        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }


    /**
     *  Read a single line. Gets the line attribute of the AccessExample object
     *  Not terribly efficient, but does the job. Works for reading a line from
     *  stdin or a file.
     *
     *@param  reader  stream from which to read the line
     *@return         either a String or null on EOF, if EOF appears in the
     *      middle of a line, returns that line, then null on next call.
     */
    String getLine(InputStreamReader reader) {

        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r') {
                    b.append((char) c);
                }
            }
        } catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0) {
            return null;
        } else {
            return b.toString();
        }
    }


    /**
     *  A simple log method.
     *
     *@param  s  The string to be logged.
     */
    private void log(String s) {

        System.out.println(s);
        System.out.flush();
    }
}
