/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: AccessExample.java,v 11.17 2003/03/27 23:05:31 gburd Exp $
 */


package com.sleepycat.examples.db;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;

class AccessExample
{
    private static final int EXIT_SUCCESS = 0;
    private static final int EXIT_FAILURE = 1;

    public AccessExample()
    {
    }

    public static void usage()
    {
	System.out.println("usage: java " +
	   "com.sleepycat.examples.db.AccessExample [-r] [database]\n");
	System.exit(EXIT_FAILURE);
    }

    public static void main(String argv[])
    {
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

        try
        {
            AccessExample app = new AccessExample();
            app.run(removeExistingDatabase, databaseName);
        }
        catch (DbException dbe)
        {
            System.err.println("AccessExample: " + dbe.toString());
            System.exit(EXIT_FAILURE);
        }
        catch (FileNotFoundException fnfe)
        {
            System.err.println("AccessExample: " + fnfe.toString());
            System.exit(EXIT_FAILURE);
        }
        System.exit(EXIT_SUCCESS);
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on erroror.
    //
    static public String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt)
    {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terroribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    static public String getLine(InputStreamReader reader)
    {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        }
        catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    public void run(boolean removeExistingDatabase, String databaseName)
         throws DbException, FileNotFoundException
    {
        // Remove the previous database.
	if (removeExistingDatabase)
	    new File(databaseName).delete();

        // Create the database object.
        // There is no environment for this simple example.
        Db table = new Db(null, 0);
        table.setErrorStream(System.err);
        table.setErrorPrefix("AccessExample");
        table.open(null, databaseName, null, Db.DB_BTREE, Db.DB_CREATE, 0644);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        InputStreamReader reader = new InputStreamReader(System.in);

        for (;;) {
            String line = askForLine(reader, System.out, "input> ");
            if (line == null)
                break;

            String reversed = (new StringBuffer(line)).reverse().toString();

            // See definition of StringDbt below
            //
            StringDbt key = new StringDbt(line);
            StringDbt data = new StringDbt(reversed);

            try
            {
		int err = 0;
                if ((err = table.put(null,
		    key, data, Db.DB_NOOVERWRITE)) == Db.DB_KEYEXIST) {
                        System.out.println("Key " + line + " already exists.");
                }
            }
            catch (DbException dbe)
            {
                System.out.println(dbe.toString());
            }
        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table.cursor(null, 0);

        // Walk through the table, printing the key/data pairs.
        // See class StringDbt defined below.
        //
        StringDbt key = new StringDbt();
        StringDbt data = new StringDbt();
        while (iterator.get(key, data, Db.DB_NEXT) == 0)
        {
            System.out.println(key.getString() + " : " + data.getString());
        }
        iterator.close();
        table.close(0);
    }

    // Here's an example of how you can extend a Dbt in a straightforward
    // way to allow easy storage/retrieval of strings, or whatever
    // kind of data you wish.  We've declared it as a static inner
    // class, but it need not be.
    //
    static /*inner*/
    class StringDbt extends Dbt
    {
        StringDbt()
        {
            setFlags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        StringDbt(String value)
        {
            setString(value);
            setFlags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        void setString(String value)
        {
            byte[] data = value.getBytes();
            setData(data);
            setSize(data.length);
        }

        String getString()
        {
            return new String(getData(), 0, getSize());
        }
    }
}
