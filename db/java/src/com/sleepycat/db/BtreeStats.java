/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class BtreeStats extends DatabaseStats {
    // no public constructor
    /* package */ BtreeStats() {}

    private int bt_magic;
    public int getMagic() {
        return bt_magic;
    }

    private int bt_version;
    public int getVersion() {
        return bt_version;
    }

    private int bt_metaflags;
    public int getMetaFlags() {
        return bt_metaflags;
    }

    private int bt_nkeys;
    public int getNumKeys() {
        return bt_nkeys;
    }

    private int bt_ndata;
    public int getNumData() {
        return bt_ndata;
    }

    private int bt_pagecnt;
    public int getPageCount() {
        return bt_pagecnt;
    }

    private int bt_pagesize;
    public int getPageSize() {
        return bt_pagesize;
    }

    private int bt_minkey;
    public int getMinKey() {
        return bt_minkey;
    }

    private int bt_re_len;
    public int getReLen() {
        return bt_re_len;
    }

    private int bt_re_pad;
    public int getRePad() {
        return bt_re_pad;
    }

    private int bt_levels;
    public int getLevels() {
        return bt_levels;
    }

    private int bt_int_pg;
    public int getIntPages() {
        return bt_int_pg;
    }

    private int bt_leaf_pg;
    public int getLeafPages() {
        return bt_leaf_pg;
    }

    private int bt_dup_pg;
    public int getDupPages() {
        return bt_dup_pg;
    }

    private int bt_over_pg;
    public int getOverPages() {
        return bt_over_pg;
    }

    private int bt_empty_pg;
    public int getEmptyPages() {
        return bt_empty_pg;
    }

    private int bt_free;
    public int getFree() {
        return bt_free;
    }

    private int bt_int_pgfree;
    public int getIntPagesFree() {
        return bt_int_pgfree;
    }

    private int bt_leaf_pgfree;
    public int getLeafPagesFree() {
        return bt_leaf_pgfree;
    }

    private int bt_dup_pgfree;
    public int getDupPagesFree() {
        return bt_dup_pgfree;
    }

    private int bt_over_pgfree;
    public int getOverPagesFree() {
        return bt_over_pgfree;
    }

    public String toString() {
        return "BtreeStats:"
            + "\n  bt_magic=" + bt_magic
            + "\n  bt_version=" + bt_version
            + "\n  bt_metaflags=" + bt_metaflags
            + "\n  bt_nkeys=" + bt_nkeys
            + "\n  bt_ndata=" + bt_ndata
            + "\n  bt_pagecnt=" + bt_pagecnt
            + "\n  bt_pagesize=" + bt_pagesize
            + "\n  bt_minkey=" + bt_minkey
            + "\n  bt_re_len=" + bt_re_len
            + "\n  bt_re_pad=" + bt_re_pad
            + "\n  bt_levels=" + bt_levels
            + "\n  bt_int_pg=" + bt_int_pg
            + "\n  bt_leaf_pg=" + bt_leaf_pg
            + "\n  bt_dup_pg=" + bt_dup_pg
            + "\n  bt_over_pg=" + bt_over_pg
            + "\n  bt_empty_pg=" + bt_empty_pg
            + "\n  bt_free=" + bt_free
            + "\n  bt_int_pgfree=" + bt_int_pgfree
            + "\n  bt_leaf_pgfree=" + bt_leaf_pgfree
            + "\n  bt_dup_pgfree=" + bt_dup_pgfree
            + "\n  bt_over_pgfree=" + bt_over_pgfree
            ;
    }
}
