/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbMpoolStat object is used to return memory pool statistics.
 *  </p>
 */
public class DbMpoolStat {
    /**
     *  Gigabytes of cache (total cache size is st_gbytes + st_bytes).
     *
     *</ul>
     *
     */
    public int st_gbytes;
    /**
     *  Bytes of cache (total cache size is st_gbytes + st_bytes).
     *
     *</ul>
     *
     */
    public int st_bytes;
    /**
     *  Number of caches.
     *</ul>
     *
     */
    public int st_ncache;
    /**
     *  Individual cache size.
     *</ul>
     *
     */
    public int st_regsize;
    /**
     *  Requested pages mapped into the process' address space (there
     *  is no available information about whether or not this request
     *  caused disk I/O, although examining the application page fault
     *  rate may be helpful).
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
     *  Clean pages forced from the cache.
     *</ul>
     *
     */
    public int st_ro_evict;
    /**
     *  Dirty pages forced from the cache.
     *</ul>
     *
     */
    public int st_rw_evict;
    /**
     *  Dirty pages written using the {@link
     *  com.sleepycat.db.DbEnv#memoryPoolTrickle
     *  DbEnv.memoryPoolTrickle} method.
     *</ul>
     *
     */
    public int st_page_trickle;
    /**
     *  Pages in the cache.
     *</ul>
     *
     */
    public int st_pages;
    /**
     *  Clean pages currently in the cache.
     *</ul>
     *
     */
    public int st_page_clean;
    /**
     *  Dirty pages currently in the cache.
     *</ul>
     *
     */
    public int st_page_dirty;
    /**
     *  Number of hash buckets in buffer hash table.
     *</ul>
     *
     */
    public int st_hash_buckets;
    /**
     *  Total number of buffer hash table lookups.
     *</ul>
     *
     */
    public int st_hash_searches;
    /**
     *  The longest chain ever encountered in buffer hash table
     *  lookups.
     *</ul>
     *
     */
    public int st_hash_longest;
    /**
     *  Total number of hash elements traversed during hash table
     *  lookups.
     *</ul>
     *
     */
    public int st_hash_examined;
    /**
     *  The number of times that a thread of control was able to
     *  obtain a hash bucket lock without waiting.
     *</ul>
     *
     */
    public int st_hash_nowait;
    /**
     *  The number of times that a thread of control was forced to
     *  wait before obtaining a hash bucket lock.
     *</ul>
     *
     */
    public int st_hash_wait;
    /**
     *  The maximum number of times any hash bucket lock was waited
     *  for by a thread of control.
     *</ul>
     *
     */
    public int st_hash_max_wait;
    /**
     *  The number of times that a thread of control was able to
     *  obtain a region lock without waiting.
     *</ul>
     *
     */
    public int st_region_nowait;
    /**
     *  The number of times that a thread of control was forced to
     *  wait before obtaining a region lock.
     *</ul>
     *
     */
    public int st_region_wait;
    /**
     *  Number of page allocations.
     *</ul>
     *
     */
    public int st_alloc;
    /**
     *  Number of hash buckets checked during allocation.
     *</ul>
     *
     */
    public int st_alloc_buckets;
    /**
     *  Maximum number of hash buckets checked during an allocation.
     *
     *</ul>
     *
     */
    public int st_alloc_max_buckets;
    /**
     *  Number of pages checked during allocation.
     *</ul>
     *
     */
    public int st_alloc_pages;
    /**
     *  Maximum number of pages checked during an allocation.
     *</ul>
     *
     */
    public int st_alloc_max_pages;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbMpoolStat:"
                + "\n  st_gbytes=" + st_gbytes
                + "\n  st_bytes=" + st_bytes
                + "\n  st_ncache=" + st_ncache
                + "\n  st_regsize=" + st_regsize
                + "\n  st_map=" + st_map
                + "\n  st_cache_hit=" + st_cache_hit
                + "\n  st_cache_miss=" + st_cache_miss
                + "\n  st_page_create=" + st_page_create
                + "\n  st_page_in=" + st_page_in
                + "\n  st_page_out=" + st_page_out
                + "\n  st_ro_evict=" + st_ro_evict
                + "\n  st_rw_evict=" + st_rw_evict
                + "\n  st_page_trickle=" + st_page_trickle
                + "\n  st_pages=" + st_pages
                + "\n  st_page_clean=" + st_page_clean
                + "\n  st_page_dirty=" + st_page_dirty
                + "\n  st_hash_buckets=" + st_hash_buckets
                + "\n  st_hash_searches=" + st_hash_searches
                + "\n  st_hash_longest=" + st_hash_longest
                + "\n  st_hash_examined=" + st_hash_examined
                + "\n  st_hash_nowait=" + st_hash_nowait
                + "\n  st_hash_wait=" + st_hash_wait
                + "\n  st_hash_max_wait=" + st_hash_max_wait
                + "\n  st_region_nowait=" + st_region_nowait
                + "\n  st_region_wait=" + st_region_wait
                + "\n  st_alloc=" + st_alloc
                + "\n  st_alloc_buckets=" + st_alloc_buckets
                + "\n  st_alloc_max_buckets=" + st_alloc_max_buckets
                + "\n  st_alloc_pages=" + st_alloc_pages
                + "\n  st_alloc_max_pages=" + st_alloc_max_pages
                ;
    }
}
// end of DbMpoolStat.java
