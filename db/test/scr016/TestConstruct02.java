/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: TestConstruct02.java,v 1.3 2001/10/05 02:36:09 bostic Exp 
 */

/*
 * Do some regression tests for constructors.
 * Run normally (without arguments) it is a simple regression test.
 * Run with a numeric argument, it repeats the regression a number
 * of times, to try to determine if there are memory leaks.
 */

package com.sleepycat.test;
import com.sleepycat.db.*;
import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;

public class TestConstruct02
{
    public static final String CONSTRUCT02_DBNAME  =       "construct02.db";
    public static final String CONSTRUCT02_DBDIR   =       "./";
    public static final String CONSTRUCT02_DBFULLPATH   =
	CONSTRUCT02_DBDIR + "/" + CONSTRUCT02_DBNAME;

    private int itemcount;	// count the number of items in the database
    public static boolean verbose_flag = false;

    private DbEnv dbenv = new DbEnv(0);

    public TestConstruct02()
        throws DbException, FileNotFoundException
    {
        dbenv.open(CONSTRUCT02_DBDIR, Db.DB_CREATE | Db.DB_INIT_MPOOL, 0666);
    }

    public void close()
    {
        try {
            dbenv.close(0);
            removeall(true, true);
        }
        catch (DbException dbe) {
            ERR("DbException: " + dbe);
        }
    }

    public static void ERR(String a)
    {
	System.out.println("FAIL: " + a);
	sysexit(1);
    }

    public static void DEBUGOUT(String s)
    {
	System.out.println(s);
    }

    public static void VERBOSEOUT(String s)
    {
        if (verbose_flag)
            System.out.println(s);
    }

    public static void sysexit(int code)
    {
	System.exit(code);
    }

    private static void check_file_removed(String name, boolean fatal,
					   boolean force_remove_first)
    {
	File f = new File(name);
	if (force_remove_first) {
	    f.delete();
	}
	if (f.exists()) {
	    if (fatal)
		System.out.print("FAIL: ");
	    System.out.print("File \"" + name + "\" still exists after run\n");
	    if (fatal)
		sysexit(1);
	}
    }


    // Check that key/data for 0 - count-1 are already present,
    // and write a key/data for count.  The key and data are
    // both "0123...N" where N == count-1.
    //
    void rundb(Db db, int count)
	throws DbException, FileNotFoundException
    {

	// The bit map of keys we've seen
	long bitmap = 0;

	// The bit map of keys we expect to see
	long expected = (1 << (count+1)) - 1;

	byte outbuf[] = new byte[count+1];
	int i;
	for (i=0; i<count; i++) {
	    outbuf[i] = (byte)('0' + i);
	    //outbuf[i] = System.out.println((byte)('0' + i);
	}
	outbuf[i++] = (byte)'x';

	/*
	 System.out.println("byte: " + ('0' + 0) + ", after: " +
	 (int)'0' + "=" + (int)('0' + 0) +
	 "," + (byte)outbuf[0]);
	 */

	Dbt key = new Dbt(outbuf, 0, i);
	Dbt data = new Dbt(outbuf, 0, i);

	//DEBUGOUT("Put: " + (char)outbuf[0] + ": " + new String(outbuf));
	db.put(null, key, data, Db.DB_NOOVERWRITE);

	// Acquire a cursor for the table.
	Dbc dbcp = db.cursor(null, 0);

	// Walk through the table, checking
	Dbt readkey = new Dbt();
	Dbt readdata = new Dbt();
	Dbt whoknows = new Dbt();

	readkey.set_flags(Db.DB_DBT_MALLOC);
	readdata.set_flags(Db.DB_DBT_MALLOC);

	//DEBUGOUT("Dbc.get");
	while (dbcp.get(readkey, readdata, Db.DB_NEXT) == 0) {
	    String key_string = new String(readkey.get_data());
	    String data_string = new String(readdata.get_data());

	    //DEBUGOUT("Got: " + key_string + ": " + data_string);
	    int len = key_string.length();
	    if (len <= 0 || key_string.charAt(len-1) != 'x') {
		ERR("reread terminator is bad");
	    }
	    len--;
	    long bit = (1 << len);
	    if (len > count) {
		ERR("reread length is bad: expect " + count + " got "+ len + " (" + key_string + ")" );
	    }
	    else if (!data_string.equals(key_string)) {
		ERR("key/data don't match");
	    }
	    else if ((bitmap & bit) != 0) {
		ERR("key already seen");
	    }
	    else if ((expected & bit) == 0) {
		ERR("key was not expected");
	    }
	    else {
		bitmap |= bit;
		expected &= ~(bit);
		for (i=0; i<len; i++) {
		    if (key_string.charAt(i) != ('0' + i)) {
			System.out.print(" got " + key_string
					 + " (" + (int)key_string.charAt(i)
					 + "), wanted " + i
					 + " (" + (int)('0' + i)
					 + ") at position " + i + "\n");
			ERR("key is corrupt");
		    }
		}
	    }
	}
	if (expected != 0) {
	    System.out.print(" expected more keys, bitmap is: " + expected + "\n");
	    ERR("missing keys in database");
	}
	dbcp.close();
    }

    void t1()
	throws DbException, FileNotFoundException
    {
	Db db = new Db(dbenv, 0);
	db.set_error_stream(System.err);
	db.set_pagesize(1024);
	db.open(CONSTRUCT02_DBNAME, null, Db.DB_BTREE,
		Db.DB_CREATE, 0664);

	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
        db.close(0);

        // Reopen no longer allowed, so we create a new db.
	db = new Db(dbenv, 0);
	db.set_error_stream(System.err);
	db.set_pagesize(1024);
	db.open(CONSTRUCT02_DBNAME, null, Db.DB_BTREE,
		Db.DB_CREATE, 0664);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
	rundb(db, itemcount++);
        db.close(0);
    }

    // remove any existing environment or database
    void removeall(boolean use_db, boolean remove_env)
    {
	{
            try {
                if (remove_env) {
                    DbEnv tmpenv = new DbEnv(0);
                    tmpenv.remove(CONSTRUCT02_DBDIR, Db.DB_FORCE);
                }
                else if (use_db) {
		    /**/
		    //memory leak for this:
		    Db tmpdb = new Db(null, 0);
		    tmpdb.remove(CONSTRUCT02_DBFULLPATH, null, 0);
		    /**/
		}
            }
            catch (DbException dbe) {
                System.err.println("error during remove: " + dbe);
            }
            catch (FileNotFoundException dbe) {
                System.err.println("error during remove: " + dbe);
            }
	}
	check_file_removed(CONSTRUCT02_DBFULLPATH, true, !use_db);
        if (remove_env) {
            for (int i=0; i<8; i++) {
                String fname = "__db.00" + i;
                check_file_removed(fname, true, !use_db);
            }
	}
    }

    boolean doall(int mask)
    {
	itemcount = 0;
	try {
	    for (int item=1; item<32; item++) {
		if ((mask & (1 << item)) != 0) {
		    VERBOSEOUT("  Running test " + item + ":\n");
		    switch (item) {
			case 1:
			    t1();
			    break;
			default:
			    ERR("unknown test case: " + item);
			    break;
		    }
		    VERBOSEOUT("  finished.\n");
		}
	    }
	    removeall((mask & 1) != 0, false);
	    return true;
	}
	catch (DbException dbe) {
	    ERR("EXCEPTION RECEIVED: " + dbe);
	}
	catch (FileNotFoundException fnfe) {
	    ERR("EXCEPTION RECEIVED: " + fnfe);
	}
	return false;
    }

    public static void main(String args[])
    {
	int iterations = 200;
	int mask = 0x3;

	for (int argcnt=0; argcnt<args.length; argcnt++) {
	    String arg = args[argcnt];
	    if (arg.charAt(0) == '-') {
                // keep on lower bit, which means to remove db between tests.
		mask = 1;
		for (int pos=1; pos<arg.length(); pos++) {
		    char ch = arg.charAt(pos);
		    if (ch >= '0' && ch <= '9') {
			mask |= (1 << (ch - '0'));
		    }
                    else if (ch == 'v') {
                        verbose_flag = true;
                    }
		    else {
			ERR("Usage:  construct02 [-testdigits] count");
		    }
		}
                System.out.println("mask = " + mask);

	    }
	    else {
		try {
		    iterations = Integer.parseInt(arg);
		    if (iterations < 0) {
			ERR("Usage:  construct02 [-testdigits] count");
		    }
		}
		catch (NumberFormatException nfe) {
		    ERR("EXCEPTION RECEIVED: " + nfe);
		}
	    }
	}

        System.gc();
        System.runFinalization();
        VERBOSEOUT("gc complete");
        long starttotal = Runtime.getRuntime().totalMemory();
        long startfree = Runtime.getRuntime().freeMemory();
        TestConstruct02 con = null;

        try {
            con = new TestConstruct02();
        }
        catch (DbException dbe) {
            System.err.println("Exception: " + dbe);
            System.exit(1);
        }
        catch (java.io.FileNotFoundException fnfe) {
            System.err.println("Exception: " + fnfe);
            System.exit(1);
        }

	for (int i=0; i<iterations; i++) {
	    if (iterations != 0) {
		VERBOSEOUT("(" + i + "/" + iterations + ") ");
	    }
	    VERBOSEOUT("construct02 running:\n");
	    if (!con.doall(mask)) {
		ERR("SOME TEST FAILED");
	    }
	    System.gc();
            System.runFinalization();
	    VERBOSEOUT("gc complete");

        }
        con.close();

        System.out.print("ALL TESTS SUCCESSFUL\n");

        long endtotal = Runtime.getRuntime().totalMemory();
        long endfree = Runtime.getRuntime().freeMemory();

        System.out.println("delta for total mem: " + magnitude(endtotal - starttotal));
        System.out.println("delta for free mem: " + magnitude(endfree - startfree));

	return;
    }

    static String magnitude(long value)
    {
        final long max = 10000000;
        for (long scale = 10; scale <= max; scale *= 10) {
            if (value < scale && value > -scale)
                return "<" + scale;
        }
        return ">" + max;
    }
}
