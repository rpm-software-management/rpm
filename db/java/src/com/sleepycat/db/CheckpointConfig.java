/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: CheckpointConfig.java,v 1.3 2004/04/21 01:09:09 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.internal.DbConstants;

public class CheckpointConfig  {
    public static final CheckpointConfig DEFAULT = new CheckpointConfig();

    private boolean force = false;
    private int kBytes = 0;
    private int minutes = 0;

    public CheckpointConfig() {
    }

    /* package */
    static CheckpointConfig checkNull(CheckpointConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    public void setKBytes(final int kBytes) {
        this.kBytes = kBytes;
    }

    public int getKBytes() {
        return kBytes;
    }

    public void setMinutes(final int minutes) {
        this.minutes = minutes;
    }

    public int getMinutes() {
        return minutes;
    }

    public void setForce(final boolean force) {
        this.force = force;
    }

    public boolean getForce() {
        return force;
    }

    /* package */
    void runCheckpoint(final DbEnv dbenv)
        throws DatabaseException {

        dbenv.txn_checkpoint(kBytes, minutes, force ? DbConstants.DB_FORCE : 0);
    }
}
