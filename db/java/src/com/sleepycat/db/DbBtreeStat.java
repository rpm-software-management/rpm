/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbBtreeStat object is used to return Btree or Recno database
 *  statistics.</p>
 */
public class DbBtreeStat {
    /**
     *  Magic number that identifies the file as a Btree database.
     *  Returned if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_magic;
    /**
     *  The version of the Btree database. Returned if Db.DB_FAST_STAT
     *  is set.
     *</ul>
     *
     */
    public int bt_version;
    public int bt_metaflags;
    /**
     *  For the Btree Access Method, the number of unique keys in the
     *  database. If Db.DB_FAST_STAT was specified and the database
     *  was created with the {@link com.sleepycat.db.Db#DB_RECNUM
     *  Db.DB_RECNUM} flag, the count will be exact, otherwise, the
     *  count will be the last saved value unless it has never been
     *  calculated, in which case it will be 0. For the Recno Access
     *  Method, the exact number of records in the database. Returned
     *  if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_nkeys;
    /**
     *  For the Btree Access Method, the number of key/data pairs in
     *  the database. If Db.DB_FAST_STAT was specified the count will
     *  be the last saved value unless it has never been calculated,
     *  in which case it will be 0. For the Recno Access Method, the
     *  exact number of records in the database. If the database has
     *  been configured to not renumber records during deletion, the
     *  count of records will only reflect undeleted records. Returned
     *  if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_ndata;
    /**
     *  Underlying database page size, in bytes. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_pagesize;
    public int bt_maxkey;
    /**
     *  The minimum keys per page. Returned if Db.DB_FAST_STAT is set.
     *
     *</ul>
     *
     */
    public int bt_minkey;
    /**
     *  The length of fixed-length records. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_re_len;
    /**
     *  The padding byte value for fixed-length records. Returned if
     *  Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int bt_re_pad;
    /**
     *  Number of levels in the database.
     *</ul>
     *
     */
    public int bt_levels;
    /**
     *  Number of database internal pages.
     *</ul>
     *
     */
    public int bt_int_pg;
    /**
     *  Number of database leaf pages.
     *</ul>
     *
     */
    public int bt_leaf_pg;
    /**
     *  Number of database duplicate pages.
     *</ul>
     *
     */
    public int bt_dup_pg;
    /**
     *  Number of database overflow pages.
     *</ul>
     *
     */
    public int bt_over_pg;
    /**
     *  Number of pages on the free list.
     *</ul>
     *
     */
    public int bt_free;
    /**
     *  Number of bytes free in database internal pages.
     *</ul>
     *
     */
    public int bt_int_pgfree;
    /**
     *  Number of bytes free in database leaf pages.
     *</ul>
     *
     */
    public int bt_leaf_pgfree;
    /**
     *  Number of bytes free in database duplicate pages.
     *</ul>
     *
     */
    public int bt_dup_pgfree;
    /**
     *  Number of bytes free in database overflow pages.
     *</ul>
     *
     */
    public int bt_over_pgfree;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbBtreeStat:"
                + "\n  bt_magic=" + bt_magic
                + "\n  bt_version=" + bt_version
                + "\n  bt_metaflags=" + bt_metaflags
                + "\n  bt_nkeys=" + bt_nkeys
                + "\n  bt_ndata=" + bt_ndata
                + "\n  bt_pagesize=" + bt_pagesize
                + "\n  bt_maxkey=" + bt_maxkey
                + "\n  bt_minkey=" + bt_minkey
                + "\n  bt_re_len=" + bt_re_len
                + "\n  bt_re_pad=" + bt_re_pad
                + "\n  bt_levels=" + bt_levels
                + "\n  bt_int_pg=" + bt_int_pg
                + "\n  bt_leaf_pg=" + bt_leaf_pg
                + "\n  bt_dup_pg=" + bt_dup_pg
                + "\n  bt_over_pg=" + bt_over_pg
                + "\n  bt_free=" + bt_free
                + "\n  bt_int_pgfree=" + bt_int_pgfree
                + "\n  bt_leaf_pgfree=" + bt_leaf_pgfree
                + "\n  bt_dup_pgfree=" + bt_dup_pgfree
                + "\n  bt_over_pgfree=" + bt_over_pgfree
                ;
    }
}
// end of DbBtreeStat.java
