/*
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/**
 * \file infcodes.h
 * Header to use infcodes.c.
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

struct inflate_codes_state;
typedef struct inflate_codes_state FAR inflate_codes_statef;

extern inflate_codes_statef *inflate_codes_new OF((
    uInt bl, uInt bd,
    inflate_huft * tl, inflate_huft * td,
    z_streamp z))
	/*@*/;

extern int inflate_codes OF((
    inflate_blocks_statef * s,
    z_streamp z,
    int r))
	/*@modifies s, z @*/;

extern void inflate_codes_free OF((
    /*@only@*/ inflate_codes_statef * s,
    z_streamp z))
	/*@*/;
