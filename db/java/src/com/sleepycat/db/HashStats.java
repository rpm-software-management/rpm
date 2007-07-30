/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class HashStats extends DatabaseStats {
    // no public constructor
    /* package */ HashStats() {}

    private int hash_magic;
    public int getMagic() {
        return hash_magic;
    }

    private int hash_version;
    public int getVersion() {
        return hash_version;
    }

    private int hash_metaflags;
    public int getMetaFlags() {
        return hash_metaflags;
    }

    private int hash_nkeys;
    public int getNumKeys() {
        return hash_nkeys;
    }

    private int hash_ndata;
    public int getNumData() {
        return hash_ndata;
    }

    private int hash_pagecnt;
    public int getPageCount() {
        return hash_pagecnt;
    }

    private int hash_pagesize;
    public int getPageSize() {
        return hash_pagesize;
    }

    private int hash_ffactor;
    public int getFfactor() {
        return hash_ffactor;
    }

    private int hash_buckets;
    public int getBuckets() {
        return hash_buckets;
    }

    private int hash_free;
    public int getFree() {
        return hash_free;
    }

    private int hash_bfree;
    public int getBFree() {
        return hash_bfree;
    }

    private int hash_bigpages;
    public int getBigPages() {
        return hash_bigpages;
    }

    private int hash_big_bfree;
    public int getBigBFree() {
        return hash_big_bfree;
    }

    private int hash_overflows;
    public int getOverflows() {
        return hash_overflows;
    }

    private int hash_ovfl_free;
    public int getOvflFree() {
        return hash_ovfl_free;
    }

    private int hash_dup;
    public int getDup() {
        return hash_dup;
    }

    private int hash_dup_free;
    public int getDupFree() {
        return hash_dup_free;
    }

    public String toString() {
        return "HashStats:"
            + "\n  hash_magic=" + hash_magic
            + "\n  hash_version=" + hash_version
            + "\n  hash_metaflags=" + hash_metaflags
            + "\n  hash_nkeys=" + hash_nkeys
            + "\n  hash_ndata=" + hash_ndata
            + "\n  hash_pagecnt=" + hash_pagecnt
            + "\n  hash_pagesize=" + hash_pagesize
            + "\n  hash_ffactor=" + hash_ffactor
            + "\n  hash_buckets=" + hash_buckets
            + "\n  hash_free=" + hash_free
            + "\n  hash_bfree=" + hash_bfree
            + "\n  hash_bigpages=" + hash_bigpages
            + "\n  hash_big_bfree=" + hash_big_bfree
            + "\n  hash_overflows=" + hash_overflows
            + "\n  hash_ovfl_free=" + hash_ovfl_free
            + "\n  hash_dup=" + hash_dup
            + "\n  hash_dup_free=" + hash_dup_free
            ;
    }
}
