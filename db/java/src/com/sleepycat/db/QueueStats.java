/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class QueueStats extends DatabaseStats {
    // no public constructor
    /* package */ QueueStats() {}

    private int qs_magic;
    public int getMagic() {
        return qs_magic;
    }

    private int qs_version;
    public int getVersion() {
        return qs_version;
    }

    private int qs_metaflags;
    public int getMetaFlags() {
        return qs_metaflags;
    }

    private int qs_nkeys;
    public int getNumKeys() {
        return qs_nkeys;
    }

    private int qs_ndata;
    public int getNumData() {
        return qs_ndata;
    }

    private int qs_pagesize;
    public int getPageSize() {
        return qs_pagesize;
    }

    private int qs_extentsize;
    public int getExtentSize() {
        return qs_extentsize;
    }

    private int qs_pages;
    public int getPages() {
        return qs_pages;
    }

    private int qs_re_len;
    public int getReLen() {
        return qs_re_len;
    }

    private int qs_re_pad;
    public int getRePad() {
        return qs_re_pad;
    }

    private int qs_pgfree;
    public int getPagesFree() {
        return qs_pgfree;
    }

    private int qs_first_recno;
    public int getFirstRecno() {
        return qs_first_recno;
    }

    private int qs_cur_recno;
    public int getCurRecno() {
        return qs_cur_recno;
    }

    public String toString() {
        return "QueueStats:"
            + "\n  qs_magic=" + qs_magic
            + "\n  qs_version=" + qs_version
            + "\n  qs_metaflags=" + qs_metaflags
            + "\n  qs_nkeys=" + qs_nkeys
            + "\n  qs_ndata=" + qs_ndata
            + "\n  qs_pagesize=" + qs_pagesize
            + "\n  qs_extentsize=" + qs_extentsize
            + "\n  qs_pages=" + qs_pages
            + "\n  qs_re_len=" + qs_re_len
            + "\n  qs_re_pad=" + qs_re_pad
            + "\n  qs_pgfree=" + qs_pgfree
            + "\n  qs_first_recno=" + qs_first_recno
            + "\n  qs_cur_recno=" + qs_cur_recno
            ;
    }
}
