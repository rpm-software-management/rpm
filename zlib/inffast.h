/*
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/**
 * \file inffast.h
 * Header to use inffast.c.
 */
/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

extern int inflate_fast OF((
    inflate_blocks_statef * s,
    z_streamp z))  // __attribute__((regparm(3)));
	/*@modifies s, z @*/;
