/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class CacheFileStats {
    // no public constructor
    /* package */ CacheFileStats() {}

    private String file_name;
    public String getFileName() {
        return file_name;
    }

    private int st_pagesize;
    public int getPageSize() {
        return st_pagesize;
    }

    private int st_map;
    public int getMap() {
        return st_map;
    }

    private int st_cache_hit;
    public int getCacheHit() {
        return st_cache_hit;
    }

    private int st_cache_miss;
    public int getCacheMiss() {
        return st_cache_miss;
    }

    private int st_page_create;
    public int getPageCreate() {
        return st_page_create;
    }

    private int st_page_in;
    public int getPageIn() {
        return st_page_in;
    }

    private int st_page_out;
    public int getPageOut() {
        return st_page_out;
    }

    public String toString() {
        return "CacheFileStats:"
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
