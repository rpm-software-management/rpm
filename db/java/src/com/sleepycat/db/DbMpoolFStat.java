/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbMpoolFStat object is used to return memory pool per-file
 *  statistics.</p>
 */
public class DbMpoolFStat {
    /**
     *  The name of the file.
     *</ul>
     *
     */
    public String file_name;
    /**
     *  Page size in bytes.
     *</ul>
     *
     */
    public int st_pagesize;
    /**
     *  Requested pages mapped into the process' address space.
     *</ul>
     *
     */
    public int st_map;
    /**
     *  Requested pages found in the cache.
     *</ul>
     *
     */
    public int st_cache_hit;
    /**
     *  Requested pages not found in the cache.
     *</ul>
     *
     */
    public int st_cache_miss;
    /**
     *  Pages created in the cache.
     *</ul>
     *
     */
    public int st_page_create;
    /**
     *  Pages read into the cache.
     *</ul>
     *
     */
    public int st_page_in;
    /**
     *  Pages written from the cache to the backing file.
     *</ul>
     *
     */
    public int st_page_out;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbMpoolFStat:"
                + "\n  file_name=" + file_name
                + "\n  st_pagesize=" + st_pagesize
                + "\n  st_map=" + st_map
                + "\n  st_cache_hit=" + st_cache_hit
                + "\n  st_cache_miss=" + st_cache_miss
                + "\n  st_page_create=" + st_page_create
                + "\n  st_page_in=" + st_page_in
                + "\n  st_page_out=" + st_page_out
                ;
    }
}
// end of DbMpoolFStat.java
