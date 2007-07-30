/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: RepConfig.java,v 1.7 2007/05/17 15:15:37 bostic Exp $
 */

package db.repquote;

import java.util.Vector;

import com.sleepycat.db.ReplicationHostAddress;
import com.sleepycat.db.ReplicationManagerStartPolicy;

public class RepConfig
{
    // Constant values used in the RepQuote application.
    public static final String progname = "RepQuoteExample";
    public static final int CACHESIZE = 10 * 1024 * 1024;
    public static final int SLEEPTIME = 5000;

    // member variables containing configuration information
    public String home; // String specifying the home directory for rep files.
    public Vector otherHosts; // stores an optional set of "other" hosts.
    public int priority; // priority within the replication group.
    public ReplicationManagerStartPolicy startPolicy;
    public ReplicationHostAddress thisHost; // The host address to listen to.
    // Optional parameter specifying the # of sites in the replication group.
    public int totalSites;
    public boolean verbose;

    // member variables used internally.
    private int currOtherHost;
    private boolean gotListenAddress;

    public RepConfig()
    {
        startPolicy = ReplicationManagerStartPolicy.REP_ELECTION;
        home = "TESTDIR";
        gotListenAddress = false;
        totalSites = 0;
        priority = 100;
        verbose = false;
        currOtherHost = 0;
        thisHost = new ReplicationHostAddress();
        otherHosts = new Vector();
    }

    public java.io.File getHome()
    {
        return new java.io.File(home);
    }

    public void setThisHost(String host, int port)
    {
        gotListenAddress = true;
        thisHost.port = port;
        thisHost.host = host;
    }

    public ReplicationHostAddress getThisHost()
    {
        if (!gotListenAddress)
            System.err.println("Warning: no host specified, returning default.");
        return thisHost;
    }

    public boolean gotListenAddress() {
        return gotListenAddress;
    }

    public void addOtherHost(String host, int port, boolean peer)
    {
        ReplicationHostAddress newInfo =
		    new ReplicationHostAddress(host, port, peer, false);
        otherHosts.add(newInfo);
    }

    public ReplicationHostAddress getFirstOtherHost()
    {
        currOtherHost = 0;
        if (otherHosts.size() == 0)
            return null;
        return (ReplicationHostAddress)otherHosts.get(currOtherHost);
    }

    public ReplicationHostAddress getNextOtherHost()
    {
        currOtherHost++;
        if (currOtherHost >= otherHosts.size())
            return null;
        return (ReplicationHostAddress)otherHosts.get(currOtherHost);
    }

    public ReplicationHostAddress getOtherHost(int i)
    {
        if (i >= otherHosts.size())
            return null;
        return (ReplicationHostAddress)otherHosts.get(i);
    }
}

