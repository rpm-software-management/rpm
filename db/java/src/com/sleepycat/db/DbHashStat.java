/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbHashStat object is used to return Hash database statistics.
 *  </p>
 */
public class DbHashStat {
    /**
     *  Magic number that identifies the file as a Hash file. Returned
     *  if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int hash_magic;
    /**
     *  The version of the Hash database. Returned if Db.DB_FAST_STAT
     *  is set.
     *</ul>
     *
     */
    public int hash_version;
    public int hash_metaflags;
    /**
     *  The number of unique keys in the database. If Db.DB_FAST_STAT
     *  was specified the count will be the last saved value unless it
     *  has never been calculated, in which case it will be 0.
     *  Returned if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int hash_nkeys;
    /**
     *  The number of key/data pairs in the database. If
     *  Db.DB_FAST_STAT was specified the count will be the last saved
     *  value unless it has never been calculated, in which case it
     *  will be 0. Returned if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int hash_ndata;
    /**
     *  The underlying Hash database page (and bucket) size, in bytes.
     *  Returned if Db.DB_FAST_STAT is set.
     *</ul>
     *
     */
    public int hash_pagesize;
    /**
     *  The desired fill factor (number of items per bucket) specified
     *  at database-creation time. Returned if Db.DB_FAST_STAT is set.
     *
     *</ul>
     *
     */
    public int hash_ffactor;
    /**
     *  The number of hash buckets. Returned if Db.DB_FAST_STAT is
     *  set.
     *</ul>
     *
     */
    public int hash_buckets;
    /**
     *  The number of pages on the free list.
     *</ul>
     *
     */
    public int hash_free;
    /**
     *  The number of bytes free on bucket pages.
     *</ul>
     *
     */
    public int hash_bfree;
    /**
     *  The number of big key/data pages.
     *</ul>
     *
     */
    public int hash_bigpages;
    /**
     *  The number of bytes free on big item pages.
     *</ul>
     *
     */
    public int hash_big_bfree;
    /**
     *  The number of overflow pages (overflow pages are pages that
     *  contain items that did not fit in the main bucket page).
     *</ul>
     *
     */
    public int hash_overflows;
    /**
     *  The number of bytes free on overflow pages.
     *</ul>
     *
     */
    public int hash_ovfl_free;
    /**
     *  The number of duplicate pages.
     *</ul>
     *
     */
    public int hash_dup;
    /**
     *  The number of bytes free on duplicate pages.
     *</ul>
     *
     */
    public int hash_dup_free;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbHashStat:"
                + "\n  hash_magic=" + hash_magic
                + "\n  hash_version=" + hash_version
                + "\n  hash_metaflags=" + hash_metaflags
                + "\n  hash_nkeys=" + hash_nkeys
                + "\n  hash_ndata=" + hash_ndata
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
// end of DbHashStat.java
