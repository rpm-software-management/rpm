/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbLogStat object is used to return logging subsystem
 *  statistics.</p>
 */
public class DbLogStat {
    /**
     *  The magic number that identifies a file as a log file.
     *</ul>
     *
     */
    public int st_magic;
    /**
     *  The version of the log file type.
     *</ul>
     *
     */
    public int st_version;
    /**
     *  The mode of any created log files.
     *</ul>
     *
     */
    public int st_mode;
    /**
     *  The in-memory log record cache size.
     *</ul>
     *
     */
    public int st_lg_bsize;
    /**
     *  The current log file size.
     *</ul>
     *
     */
    public int st_lg_size;
    /**
     *  The number of bytes over and above <b>st_w_mbytes</b> written
     *  to this log.
     *</ul>
     *
     */
    public int st_w_bytes;
    /**
     *  The number of megabytes written to this log.
     *</ul>
     *
     */
    public int st_w_mbytes;
    /**
     *  The number of bytes over and above <b>st_wc_mbytes</b> written
     *  to this log since the last checkpoint.
     *</ul>
     *
     */
    public int st_wc_bytes;
    /**
     *  The number of megabytes written to this log since the last
     *  checkpoint.
     *</ul>
     *
     */
    public int st_wc_mbytes;
    /**
     *  The number of times the log has been written to disk.
     *</ul>
     *
     */
    public int st_wcount;
    /**
     *  The number of times the log has been written to disk because
     *  the in-memory log record cache filled up.
     *</ul>
     *
     */
    public int st_wcount_fill;
    /**
     *  The number of times the log has been flushed to disk.
     *</ul>
     *
     */
    public int st_scount;
    /**
     *  The number of times that a thread of control was forced to
     *  wait before obtaining the region lock.
     *</ul>
     *
     */
    public int st_region_wait;
    /**
     *  The number of times that a thread of control was able to
     *  obtain the region lock without waiting.
     *</ul>
     *
     */
    public int st_region_nowait;
    /**
     *  The current log file number.
     *</ul>
     *
     */
    public int st_cur_file;
    /**
     *  The byte offset in the current log file.
     *</ul>
     *
     */
    public int st_cur_offset;
    /**
     *  The log file number of the last record known to be on disk.
     *
     *</ul>
     *
     */
    public int st_disk_file;
    /**
     *  The byte offset of the last record known to be on disk.
     *</ul>
     *
     */
    public int st_disk_offset;
    /**
     *  The size of the region.
     *</ul>
     *
     */
    public int st_regsize;
    /**
     *  The maximum number of commits contained in a single log flush.
     *
     *</ul>
     *
     */
    public int st_maxcommitperflush;
    /**
     *  The minimum number of commits contained in a single log flush
     *  that contained a commit.
     *</ul>
     *
     */
    public int st_mincommitperflush;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbLogStat:"
                + "\n  st_magic=" + st_magic
                + "\n  st_version=" + st_version
                + "\n  st_mode=" + st_mode
                + "\n  st_lg_bsize=" + st_lg_bsize
                + "\n  st_lg_size=" + st_lg_size
                + "\n  st_w_bytes=" + st_w_bytes
                + "\n  st_w_mbytes=" + st_w_mbytes
                + "\n  st_wc_bytes=" + st_wc_bytes
                + "\n  st_wc_mbytes=" + st_wc_mbytes
                + "\n  st_wcount=" + st_wcount
                + "\n  st_wcount_fill=" + st_wcount_fill
                + "\n  st_scount=" + st_scount
                + "\n  st_region_wait=" + st_region_wait
                + "\n  st_region_nowait=" + st_region_nowait
                + "\n  st_cur_file=" + st_cur_file
                + "\n  st_cur_offset=" + st_cur_offset
                + "\n  st_disk_file=" + st_disk_file
                + "\n  st_disk_offset=" + st_disk_offset
                + "\n  st_regsize=" + st_regsize
                + "\n  st_maxcommitperflush=" + st_maxcommitperflush
                + "\n  st_mincommitperflush=" + st_mincommitperflush
                ;
    }
}
// end of DbLogStat.java
