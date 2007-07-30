/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class CacheStats {
    // no public constructor
    /* package */ CacheStats() {}

    private int st_gbytes;
    public int getGbytes() {
        return st_gbytes;
    }

    private int st_bytes;
    public int getBytes() {
        return st_bytes;
    }

    private int st_ncache;
    public int getNumCache() {
        return st_ncache;
    }

    private int st_max_ncache;
    public int getMaxNumCache() {
        return st_max_ncache;
    }

    private int st_mmapsize;
    public int getMmapSize() {
        return st_mmapsize;
    }

    private int st_maxopenfd;
    public int getMaxOpenfd() {
        return st_maxopenfd;
    }

    private int st_maxwrite;
    public int getMaxWrite() {
        return st_maxwrite;
    }

    private int st_maxwrite_sleep;
    public int getMaxWriteSleep() {
        return st_maxwrite_sleep;
    }

    private int st_pages;
    public int getPages() {
        return st_pages;
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

    private int st_ro_evict;
    public int getRoEvict() {
        return st_ro_evict;
    }

    private int st_rw_evict;
    public int getRwEvict() {
        return st_rw_evict;
    }

    private int st_page_trickle;
    public int getPageTrickle() {
        return st_page_trickle;
    }

    private int st_page_clean;
    public int getPageClean() {
        return st_page_clean;
    }

    private int st_page_dirty;
    public int getPageDirty() {
        return st_page_dirty;
    }

    private int st_hash_buckets;
    public int getHashBuckets() {
        return st_hash_buckets;
    }

    private int st_hash_searches;
    public int getHashSearches() {
        return st_hash_searches;
    }

    private int st_hash_longest;
    public int getHashLongest() {
        return st_hash_longest;
    }

    private int st_hash_examined;
    public int getHashExamined() {
        return st_hash_examined;
    }

    private int st_hash_nowait;
    public int getHashNowait() {
        return st_hash_nowait;
    }

    private int st_hash_wait;
    public int getHashWait() {
        return st_hash_wait;
    }

    private int st_hash_max_nowait;
    public int getHashMaxNowait() {
        return st_hash_max_nowait;
    }

    private int st_hash_max_wait;
    public int getHashMaxWait() {
        return st_hash_max_wait;
    }

    private int st_region_nowait;
    public int getRegionNowait() {
        return st_region_nowait;
    }

    private int st_region_wait;
    public int getRegionWait() {
        return st_region_wait;
    }

    private int st_mvcc_frozen;
    public int getMultiversionFrozen() {
        return st_mvcc_frozen;
    }

    private int st_mvcc_thawed;
    public int getMultiversionThawed() {
        return st_mvcc_thawed;
    }

    private int st_mvcc_freed;
    public int getMultiversionFreed() {
        return st_mvcc_freed;
    }

    private int st_alloc;
    public int getAlloc() {
        return st_alloc;
    }

    private int st_alloc_buckets;
    public int getAllocBuckets() {
        return st_alloc_buckets;
    }

    private int st_alloc_max_buckets;
    public int getAllocMaxBuckets() {
        return st_alloc_max_buckets;
    }

    private int st_alloc_pages;
    public int getAllocPages() {
        return st_alloc_pages;
    }

    private int st_alloc_max_pages;
    public int getAllocMaxPages() {
        return st_alloc_max_pages;
    }

    private int st_io_wait;
    public int getIoWait() {
        return st_io_wait;
    }

    private int st_regsize;
    public int getRegSize() {
        return st_regsize;
    }

    public String toString() {
        return "CacheStats:"
            + "\n  st_gbytes=" + st_gbytes
            + "\n  st_bytes=" + st_bytes
            + "\n  st_ncache=" + st_ncache
            + "\n  st_max_ncache=" + st_max_ncache
            + "\n  st_mmapsize=" + st_mmapsize
            + "\n  st_maxopenfd=" + st_maxopenfd
            + "\n  st_maxwrite=" + st_maxwrite
            + "\n  st_maxwrite_sleep=" + st_maxwrite_sleep
            + "\n  st_pages=" + st_pages
            + "\n  st_map=" + st_map
            + "\n  st_cache_hit=" + st_cache_hit
            + "\n  st_cache_miss=" + st_cache_miss
            + "\n  st_page_create=" + st_page_create
            + "\n  st_page_in=" + st_page_in
            + "\n  st_page_out=" + st_page_out
            + "\n  st_ro_evict=" + st_ro_evict
            + "\n  st_rw_evict=" + st_rw_evict
            + "\n  st_page_trickle=" + st_page_trickle
            + "\n  st_page_clean=" + st_page_clean
            + "\n  st_page_dirty=" + st_page_dirty
            + "\n  st_hash_buckets=" + st_hash_buckets
            + "\n  st_hash_searches=" + st_hash_searches
            + "\n  st_hash_longest=" + st_hash_longest
            + "\n  st_hash_examined=" + st_hash_examined
            + "\n  st_hash_nowait=" + st_hash_nowait
            + "\n  st_hash_wait=" + st_hash_wait
            + "\n  st_hash_max_nowait=" + st_hash_max_nowait
            + "\n  st_hash_max_wait=" + st_hash_max_wait
            + "\n  st_region_nowait=" + st_region_nowait
            + "\n  st_region_wait=" + st_region_wait
            + "\n  st_mvcc_frozen=" + st_mvcc_frozen
            + "\n  st_mvcc_thawed=" + st_mvcc_thawed
            + "\n  st_mvcc_freed=" + st_mvcc_freed
            + "\n  st_alloc=" + st_alloc
            + "\n  st_alloc_buckets=" + st_alloc_buckets
            + "\n  st_alloc_max_buckets=" + st_alloc_max_buckets
            + "\n  st_alloc_pages=" + st_alloc_pages
            + "\n  st_alloc_max_pages=" + st_alloc_max_pages
            + "\n  st_io_wait=" + st_io_wait
            + "\n  st_regsize=" + st_regsize
            ;
    }
}
