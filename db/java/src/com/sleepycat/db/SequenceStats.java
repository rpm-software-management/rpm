/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class SequenceStats {
    // no public constructor
    /* package */ SequenceStats() {}

    private int st_wait;
    public int getWait() {
        return st_wait;
    }

    private int st_nowait;
    public int getNowait() {
        return st_nowait;
    }

    private long st_current;
    public long getCurrent() {
        return st_current;
    }

    private long st_value;
    public long getValue() {
        return st_value;
    }

    private long st_last_value;
    public long getLastValue() {
        return st_last_value;
    }

    private long st_min;
    public long getMin() {
        return st_min;
    }

    private long st_max;
    public long getMax() {
        return st_max;
    }

    private int st_cache_size;
    public int getCacheSize() {
        return st_cache_size;
    }

    private int st_flags;
    public int getFlags() {
        return st_flags;
    }

    public String toString() {
        return "SequenceStats:"
            + "\n  st_wait=" + st_wait
            + "\n  st_nowait=" + st_nowait
            + "\n  st_current=" + st_current
            + "\n  st_value=" + st_value
            + "\n  st_last_value=" + st_last_value
            + "\n  st_min=" + st_min
            + "\n  st_max=" + st_max
            + "\n  st_cache_size=" + st_cache_size
            + "\n  st_flags=" + st_flags
            ;
    }
}
