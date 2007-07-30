/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: SequenceConfig.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbSequence;
import com.sleepycat.db.internal.DbTxn;

public class SequenceConfig implements Cloneable {
    /*
     * For internal use, final to allow null as a valid value for
     * the config parameter.
     */
    public static final SequenceConfig DEFAULT = new SequenceConfig();

    /* package */
    static SequenceConfig checkNull(SequenceConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    /* Parameters */
    private int cacheSize = 0;
    private long rangeMin = Long.MIN_VALUE;
    private long rangeMax = Long.MAX_VALUE;
    private long initialValue = 0L;

    /* Flags */
    private boolean allowCreate = false;
    private boolean decrement = false;
    private boolean exclusiveCreate = false;
    private boolean autoCommitNoSync = false;
    private boolean wrap = false;

    public SequenceConfig() {
    }

    public void setAllowCreate(final boolean allowCreate) {
        this.allowCreate = allowCreate;
    }

    public boolean getAllowCreate() {
        return allowCreate;
    }

    public void setCacheSize(final int cacheSize) {
        this.cacheSize = cacheSize;
    }

    public int getCacheSize() {
        return cacheSize;
    }

    public void setDecrement(boolean decrement) {
        this.decrement = decrement;
    }

    public boolean getDecrement() {
         return decrement;
    }

    public void setExclusiveCreate(final boolean exclusiveCreate) {
        this.exclusiveCreate = exclusiveCreate;
    }

    public boolean getExclusiveCreate() {
        return exclusiveCreate;
    }

    public void setInitialValue(long initialValue) {
        this.initialValue = initialValue;
    }

    public long getInitialValue() {
        return initialValue;
    }

    public void setAutoCommitNoSync(final boolean autoCommitNoSync) {
        this.autoCommitNoSync = autoCommitNoSync;
    }

    public boolean getAutoCommitNoSync() {
        return autoCommitNoSync;
    }

    public void setRange(final long min, final long max) {
        this.rangeMin = min;
        this.rangeMax = max;
    }

    public long getRangeMin() {
        return rangeMin;
    }

    public long getRangeMax() {
        return rangeMax;
    }

    public void setWrap(final boolean wrap) {
        this.wrap = wrap;
    }

    public boolean getWrap() {
        return wrap;
    }

    /* package */
    DbSequence createSequence(final Db db)
        throws DatabaseException {

        int createFlags = 0;

        return new DbSequence(db, createFlags);
    }

    /* package */
    DbSequence openSequence(final Db db,
                    final DbTxn txn,
                    final DatabaseEntry key)
        throws DatabaseException {

        final DbSequence seq = createSequence(db);
        // The DB_THREAD flag is inherited from the database
        boolean threaded = ((db.get_open_flags() & DbConstants.DB_THREAD) != 0);

        int openFlags = 0;
        openFlags |= allowCreate ? DbConstants.DB_CREATE : 0;
        openFlags |= exclusiveCreate ? DbConstants.DB_EXCL : 0;
        openFlags |= threaded ? DbConstants.DB_THREAD : 0;

        if (db.get_transactional() && txn == null)
            openFlags |= DbConstants.DB_AUTO_COMMIT;

        configureSequence(seq, DEFAULT);
        boolean succeeded = false;
        try {
            seq.open(txn, key, openFlags);
            succeeded = true;
            return seq;
        } finally {
            if (!succeeded)
                try {
                    seq.close(0);
                } catch (Throwable t) {
                    // Ignore it -- an exception is already in flight.
                }
        }
    }

    /* package */
    void configureSequence(final DbSequence seq, final SequenceConfig oldConfig)
        throws DatabaseException {

        int seqFlags = 0;
        seqFlags |= decrement ? DbConstants.DB_SEQ_DEC : DbConstants.DB_SEQ_INC;
        seqFlags |= wrap ? DbConstants.DB_SEQ_WRAP : 0;

        if (seqFlags != 0)
            seq.set_flags(seqFlags);

        if (rangeMin != oldConfig.rangeMin || rangeMax != oldConfig.rangeMax)
            seq.set_range(rangeMin, rangeMax);

        if (initialValue != oldConfig.initialValue)
            seq.initial_value(initialValue);

        if (cacheSize != oldConfig.cacheSize)
            seq.set_cachesize(cacheSize);
    }

    /* package */
    SequenceConfig(final DbSequence seq)
        throws DatabaseException {

        // XXX: can't get open flags
        final int openFlags = 0;
        allowCreate = (openFlags & DbConstants.DB_CREATE) != 0;
        exclusiveCreate = (openFlags & DbConstants.DB_EXCL) != 0;

        final int seqFlags = seq.get_flags();
        decrement = (seqFlags & DbConstants.DB_SEQ_DEC) != 0;
        wrap = (seqFlags & DbConstants.DB_SEQ_WRAP) != 0;

        // XXX: can't get initial value
        final long initialValue = 0;

        cacheSize = seq.get_cachesize();

        rangeMin = seq.get_range_min();
        rangeMax = seq.get_range_max();
    }
}
