/*
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/**
 * \file infblock.h
 * Header to use infblock.c.
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

struct inflate_blocks_state;
typedef struct inflate_blocks_state FAR inflate_blocks_statef;

/*@null@*/
extern inflate_blocks_statef * inflate_blocks_new OF((
    z_streamp z,
    check_func c,               /* check function */
    uInt w))                    /* window size */
	/*@modifies z @*/;

extern int inflate_blocks OF((
    inflate_blocks_statef * s,
    z_streamp z,
    int r))                     /* initial return code */
	/*@modifies s, z @*/;

extern void inflate_blocks_reset OF((
    inflate_blocks_statef * s,
    z_streamp z,
    uLongf * c))                /* check value on output */
	/*@modifies s, *c, z @*/;

extern int inflate_blocks_free OF((
    /*@only@*/ inflate_blocks_statef * s,
    z_streamp z))
	/*@modifies s, z @*/;

extern void inflate_set_dictionary OF((
    inflate_blocks_statef * s,
    const Bytef * d, /* dictionary */
    uInt  n))        /* dictionary length */
	/*@modifies s @*/;

extern int inflate_blocks_sync_point OF((
    inflate_blocks_statef *s))
	/*@modifies s @*/;
