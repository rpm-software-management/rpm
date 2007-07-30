/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: VerboseConfig.java,v 12.4 2007/07/06 00:22:54 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;

public final class VerboseConfig {
    public static final VerboseConfig DEADLOCK =
        new VerboseConfig("DEADLOCK", DbConstants.DB_VERB_DEADLOCK);
    public static final VerboseConfig FILEOPS =
        new VerboseConfig("FILEOPS", DbConstants.DB_VERB_FILEOPS);
    public static final VerboseConfig FILEOPS_ALL =
        new VerboseConfig("FILEOPS_ALL", DbConstants.DB_VERB_FILEOPS_ALL);
    public static final VerboseConfig RECOVERY =
        new VerboseConfig("RECOVERY", DbConstants.DB_VERB_RECOVERY);
    public static final VerboseConfig REGISTER =
        new VerboseConfig("REGISTER", DbConstants.DB_VERB_REGISTER);
    public static final VerboseConfig REPLICATION =
        new VerboseConfig("REPLICATION", DbConstants.DB_VERB_REPLICATION);
    public static final VerboseConfig WAITSFOR =
        new VerboseConfig("WAITSFOR", DbConstants.DB_VERB_WAITSFOR);

    /* Package */
    int getInternalFlag() {
        return verboseFlag;
    }
    /* For toString */
    private String verboseName;
    private int verboseFlag;

    private VerboseConfig(final String verboseName, int verboseFlag) {
        this.verboseName = verboseName;
        this.verboseFlag = verboseFlag;
    }

    public String toString() {
        return "VerboseConfig." + verboseName;
    }
}

