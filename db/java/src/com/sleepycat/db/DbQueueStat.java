/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbQueueStat object is used to return Queue database
 *  statistics.</p>
 */
public class DbQueueStat {
    /**
     *  Magic number that identifies the file as a Queue file.
     *  Returned if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_magic;
    /**
     *  The version of the Queue file type. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_version;
    public int qs_metaflags;
    /**
     *  The number of records in the database. If Db.DB_FAST_STAT was
     *  specified the count will be the last saved value unless it has
     *  never been calculated, in which case it will be 0. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_nkeys;
    /**
     *  The number of records in the database. If Db.DB_FAST_STAT was
     *  specified the count will be the last saved value unless it has
     *  never been calculated, in which case it will be 0. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_ndata;
    /**
     *  Underlying database page size, in bytes. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_pagesize;
    /**
     *  Underlying database extent size, in pages. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_extentsize;
    /**
     *  Number of pages in the database.
     *</ul>
     *
     */
    public int qs_pages;
    /**
     *  The length of the records. Returned if Db.DB_FAST_STAT is set.
     *
     *</ul>
     *
     */
    public int qs_re_len;
    /**
     *  The padding byte value for the records. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_re_pad;
    /**
     *  Number of bytes free in database pages.
     *</ul>
     *
     */
    public int qs_pgfree;
    /**
     *  First undeleted record in the database. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int qs_first_recno;
    /**
     *  Next available record number. Returned if Db.DB_FAST_STAT is
     *  set.
     *</ul>
     *
     */
    public int qs_cur_recno;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbQueueStat:"
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
// end of DbQueueStat.java
