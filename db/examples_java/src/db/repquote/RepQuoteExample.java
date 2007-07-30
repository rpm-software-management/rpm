/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: RepQuoteExample.java,v 1.17 2007/05/17 18:17:19 bostic Exp $
 */

package db.repquote;

import java.io.FileNotFoundException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.lang.Thread;
import java.lang.InterruptedException;

import com.sleepycat.db.*;
import db.repquote.RepConfig;

/**
 * RepQuoteExample is a simple but complete demonstration of a replicated
 * application. The application is a mock stock ticker. The master accepts a
 * stock symbol and an numerical value as input, and stores this information
 * into a replicated database; either master or clients can display the
 * contents of the database.
 * <p>
 * The options to start a given replication node are:
 * <pre>
 *   -M (configure this process to start as a master)
 *   -C (configure this process to start as a client)
 *   -h environment home directory
 *   -m host:port (required; m stands for me)
 *   -o host:port (optional; o stands for other; any number of these may
 *      be specified)
 *   -f host:port (optional; f stands for friend, and indicates a peer
 *      relationship to the specified site)
 *   -n nsites (optional; number of sites in replication group.
 *      Defaults to 0 in which case we dynamically compute the number of
 *      sites in the replication group)
 *   -p priority (optional: defaults to 100)
 *   -v Enable verbose logging
 * </pre>
 * <p>
 * A typical session begins with a command such as the following to start a
 * master:
 *
 * <pre>
 *   java db.repquote.RepQuoteExample -M -h dir1 -m localhost:6000
 * </pre>
 *
 * and several clients:
 *
 * <pre>
 *   java db.repquote.RepQuoteExample -C -h dir2
 *               -m localhost:6001 -o localhost:6000
 *   java db.repquote.RepQuoteExample -C -h dir3
 *               -m localhost:6002 -o localhost:6000
 *   java db.repquote.RepQuoteExample -C -h dir4
 *               -m localhost:6003 -o localhost:6000
 * </pre>
 *
 * <p>
 * Each process is a member of a DB replication group. The sample application
 * expects the following commands to stdin:
 * <ul>
 * <li>NEWLINE -- print all the stocks held in the database</li>
 * <li>quit -- shutdown this node</li>
 * <li>exit -- shutdown this node</li>
 * <li>stock_symbol number -- enter this stock and number into the
 * database</li>
 * </ul>
 */

public class RepQuoteExample
{
    private RepConfig appConfig;
    private RepQuoteEnvironment dbenv;

    public static void usage()
    {
        System.err.println("usage: " + RepConfig.progname);
        System.err.println("[-C][-M][-F][-h home][-o host:port]" +
            "[-m host:port][-f host:port][-n nsites][-p priority][-v]");

        System.err.println(
             "\t -C start the site as client of the replication group\n" +
             "\t -M start the site as master of the replication group\n" +
             "\t -f host:port (optional; f stands for friend and \n" +
             "\t    indicates a peer relationship to the specified site)\n" +
             "\t -h home directory\n" +
             "\t -m host:port (required; m stands for me)\n" +
             "\t -n nsites (optional; number of sites in replication \n" +
             "\t    group; defaults to 0\n" +
             "\t    In which case the number of sites are computed \n" +
             "\t    dynamically\n" +
             "\t -o host:port (optional; o stands for other; any number\n" +
             "\t    of these may be specified)\n" +
             "\t -p priority (optional: defaults to 100)\n" +
             "\t -v Enable verbose logging\n");

        System.exit(1);
    }

    public static void main(String[] argv)
        throws Exception
    {
        RepConfig config = new RepConfig();
        boolean isPeer;
        String tmpHost;
        int tmpPort = 0;
        // Extract the command line parameters
        for (int i = 0; i < argv.length; i++)
        {
            isPeer = false;
            if (argv[i].compareTo("-C") == 0) {
                config.startPolicy = ReplicationManagerStartPolicy.REP_CLIENT;
            } else if (argv[i].compareTo("-h") == 0) {
                // home - a string arg.
                i++;
                config.home = argv[i];
            } else if (argv[i].compareTo("-M") == 0) {
                config.startPolicy = ReplicationManagerStartPolicy.REP_MASTER;
            } else if (argv[i].compareTo("-m") == 0) {
                // "me" should be host:port
                i++;
                String[] words = argv[i].split(":");
                if (words.length != 2) {
                    System.err.println(
                        "Invalid host specification host:port needed.");
                    usage();
                }
                try {
                    tmpPort = Integer.parseInt(words[1]);
                } catch (NumberFormatException nfe) {
                    System.err.println("Invalid host specification, " +
                        "could not parse port number.");
                    usage();
                }
                config.setThisHost(words[0], tmpPort);
            } else if (argv[i].compareTo("-n") == 0) {
                i++;
                config.totalSites = Integer.parseInt(argv[i]);
            } else if (argv[i].compareTo("-f") == 0 ||
                       argv[i].compareTo("-o") == 0) {
                if (argv[i] == "-f")
                    isPeer = true;
                i++;
                String[] words = argv[i].split(":");
                if (words.length != 2) {
                    System.err.println(
                        "Invalid host specification host:port needed.");
                    usage();
                }
                try {
                    tmpPort = Integer.parseInt(words[1]);
                } catch (NumberFormatException nfe) {
                    System.err.println("Invalid host specification, " +
                        "could not parse port number.");
                    usage();
                }
                config.addOtherHost(words[0], tmpPort, isPeer);
            } else if (argv[i].compareTo("-p") == 0) {
                i++;
                config.priority = Integer.parseInt(argv[i]);
            } else if (argv[i].compareTo("-v") == 0) {
                config.verbose = true;
            } else {
                System.err.println("Unrecognized option: " + argv[i]);
                usage();
            }

        }

        // Error check command line.
        if ((!config.gotListenAddress()) || config.home.length() == 0)
            usage();

        RepQuoteExample runner = null;
        try {
            runner = new RepQuoteExample();
            runner.init(config);

            // Sleep to give ourselves time to find a master.
            //try {
            //    Thread.sleep(5000);
            //} catch (InterruptedException e) {}

            runner.doloop();
            runner.terminate();
        } catch (DatabaseException dbe) {
            System.err.println("Caught an exception during " +
                "initialization or processing: " + dbe);
            if (runner != null)
                runner.terminate();
        }
    } // end main

    public RepQuoteExample()
        throws DatabaseException
    {
        appConfig = null;
        dbenv = null;
    }

    public int init(RepConfig config)
        throws DatabaseException
    {
        int ret = 0;
        appConfig = config;
        EnvironmentConfig envConfig = new EnvironmentConfig();
        envConfig.setErrorStream(System.err);
        envConfig.setErrorPrefix(RepConfig.progname);

        envConfig.setReplicationManagerLocalSite(appConfig.getThisHost());
        for (ReplicationHostAddress host = appConfig.getFirstOtherHost();
            host != null; host = appConfig.getNextOtherHost())
        {
            envConfig.replicationManagerAddRemoteSite(host);
        }

        if (appConfig.totalSites > 0)
            envConfig.setReplicationNumSites(appConfig.totalSites);
        envConfig.setReplicationPriority(appConfig.priority);

         envConfig.setCacheSize(RepConfig.CACHESIZE);
        envConfig.setTxnNoSync(true);

        envConfig.setEventHandler(new RepQuoteEventHandler());
        envConfig.setReplicationManagerAckPolicy(ReplicationManagerAckPolicy.ALL);

        envConfig.setAllowCreate(true);
        envConfig.setRunRecovery(true);
        envConfig.setThreaded(true);
        envConfig.setInitializeReplication(true);
        envConfig.setInitializeLocking(true);
        envConfig.setInitializeLogging(true);
        envConfig.setInitializeCache(true);
        envConfig.setTransactional(true);
        envConfig.setVerboseReplication(appConfig.verbose);
        try {
            dbenv = new RepQuoteEnvironment(appConfig.getHome(), envConfig);
        } catch(FileNotFoundException e) {
            System.err.println("FileNotFound exception: " + e);
            System.err.println(
                "Ensure that the environment directory is pre-created.");
            ret = 1;
        }

        // start replication manager
        dbenv.replicationManagerStart(3, appConfig.startPolicy);
        return ret;
    }

    public int doloop()
        throws DatabaseException
    {
        Database db = null;

        for (;;)
        {
            if (db == null) {
                DatabaseConfig dbconf = new DatabaseConfig();
                // Set page size small so page allocation is cheap.
                dbconf.setPageSize(512);
                dbconf.setType(DatabaseType.BTREE);
                if (dbenv.getIsMaster()) {
                    dbconf.setAllowCreate(true);
                }
                dbconf.setTransactional(true);

                try {
                    db = dbenv.openDatabase
                        (null, RepConfig.progname, null, dbconf);
                } catch (java.io.FileNotFoundException e) {
                    System.err.println("no stock database available yet.");
                    if (db != null) {
                        db.close(true);
                        db = null;
                    }
                    try {
                        Thread.sleep(RepConfig.SLEEPTIME);
                    } catch (InterruptedException ie) {}
                    continue;
                }
            }

            BufferedReader stdin =
                new BufferedReader(new InputStreamReader(System.in));

            // listen for input, and add it to the database.
            System.out.print("QUOTESERVER");
            if (!dbenv.getIsMaster())
                System.out.print("(read-only)");
            System.out.print("> ");
            System.out.flush();
            String nextline = null;
            try {
                nextline = stdin.readLine();
            } catch (IOException ioe) {
                System.err.println("Unable to get data from stdin");
                break;
            }
            String[] words = nextline.split("\\s");

            // A blank line causes the DB to be dumped to stdout.
            if (words.length == 0 ||
                (words.length == 1 && words[0].length() == 0)) {
                try {
                    printStocks(db);
                } catch (DeadlockException de) {
                    continue;
                } catch (DatabaseException e) {
                    // this could be DB_REP_HANDLE_DEAD
                    // should close the database and continue
                    System.err.println("Got db exception reading replication" +
                        "DB: " + e);
                    System.err.println("Expected if it was due to a dead " +
                        "replication handle, otherwise an unexpected error.");
                    db.close(true); // close no sync.
                    db = null;
                    continue;
                }
                continue;
            }

            if (words.length == 1 &&
                (words[0].compareToIgnoreCase("quit") == 0 ||
                words[0].compareToIgnoreCase("exit") == 0)) {
                break;
            } else if (words.length != 2) {
                System.err.println("Format: TICKER VALUE");
                continue;
            }

            if (!dbenv.getIsMaster()) {
                System.err.println("Can't update client.");
                continue;
            }

            DatabaseEntry key = new DatabaseEntry(words[0].getBytes());
            DatabaseEntry data = new DatabaseEntry(words[1].getBytes());

            db.put(null, key, data);
        }
        if (db != null)
            db.close(true);
        return 0;
    }

    public void terminate()
        throws DatabaseException
    {
        dbenv.close();
    }

    /*
     * void return type since error conditions are propogated
     * via exceptions.
     */
    private void printStocks(Database db)
        throws DeadlockException, DatabaseException
    {
        Cursor dbc = db.openCursor(null, null);

        System.out.println("\tSymbol\tPrice");
        System.out.println("\t======\t=====");

        DatabaseEntry key = new DatabaseEntry();
        DatabaseEntry data = new DatabaseEntry();
        OperationStatus ret;
        for (ret = dbc.getFirst(key, data, LockMode.DEFAULT);
            ret == OperationStatus.SUCCESS;
            ret = dbc.getNext(key, data, LockMode.DEFAULT)) {
            String keystr = new String
                (key.getData(), key.getOffset(), key.getSize());
            String datastr = new String
                (data.getData(), data.getOffset(), data.getSize());
            System.out.println("\t"+keystr+"\t"+datastr);

        }
        dbc.close();
    }

    /*
     * Implemention of EventHandler interface to handle the Berkeley DB events
     * we are interested in receiving.
     */
    private /* internal */
    class RepQuoteEventHandler extends EventHandlerAdapter {
        public void handleRepClientEvent()
	{
	    dbenv.setIsMaster(false);
	}
        public void handleRepMasterEvent()
	{
	    dbenv.setIsMaster(true);
	}
    }
} // end class

