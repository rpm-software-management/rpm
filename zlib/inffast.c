/*
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/**
 * \file inffast.c
 * Process literals and length/distance pairs fast.
 */

#include "zutil.h"
#include "inftrees.h"
#include "infblock.h"
#include "infcodes.h"
#include "infutil.h"
#include "inffast.h"
#include "crc32.h"

/* simplify the use of the inflate_huft type with some defines */
#define exop word.what.Exop
#define bits word.what.Bits

/* macros for bit input with no checking and for returning unused bytes */
#define GRABBITS(j) {\
	if (k+8 < (j)) { b |= NEXTSHORT << k; k += 16; } \
	if (k<(j)){b|=((uLong)NEXTBYTE)<<k;k+=8;}}
#define UNGRAB {c=z->avail_in-n;c=(k>>3)<c?k>>3:c;n+=c;p-=c;k-=c<<3;}

/* Called with number of bytes left to write in window at least 258
   (the maximum string length) and number of input bytes available
   at least ten.  The ten bytes are six bytes for the longest length/
   distance pair plus four bytes for overloading the bit buffer. */

int inflate_fast(inflate_blocks_statef *s, z_streamp z)
{
inflate_codes_statef *sc = s->sub.decode.codes;
uInt bl = sc->lbits, bd = sc->dbits;
inflate_huft *tl = sc->ltree;
inflate_huft *td = sc->dtree; /* need separate declaration for Borland C++ */
  inflate_huft *t;      /* temporary pointer */
  uInt e;               /* extra bits or operation */
  uLong b;              /* bit buffer */
  uInt k;               /* bits in bit buffer */
  Bytef *p;             /* input data pointer */
  uInt n;               /* bytes available there */
  Bytef *q;             /* output window write pointer */
  uInt m;               /* bytes to end of window or read pointer */
  uInt ml;              /* mask for literal/length tree */
  uInt md;              /* mask for distance tree */
  uInt c;               /* bytes to copy */
  uInt d;               /* distance back to copy from */
  Bytef *r;             /* copy source pointer */
int ret;

  /* load input, output, bit values */
  LOAD

/*@-unrecog@*/
  PREFETCH(p);
  PREFETCH(p+32);
/*@=unrecog@*/
  /* initialize masks */
  ml = inflate_mask[bl];
  md = inflate_mask[bd];

  /* do until not enough input or output space for fast loop */
  do {                          /* assume called with m >= 258 && n >= 10 */
    /* get literal/length code */
/*@-unrecog@*/
    PREFETCH(p+64);
/*@=unrecog@*/
    GRABBITS(20)                /* max bits for literal/length code */
    if ((e = (t = tl + ((uInt)b & ml))->exop) == 0)
    {
      DUMPBITS(t->bits)
      Tracevv((stderr, t->base >= 0x20 && t->base < 0x7f ?
                "inflate:         * literal '%c'\n" :
                "inflate:         * literal 0x%02x\n", t->base));
      OUTBYTE((Byte)t->base);
      continue;
    }
    do {
      DUMPBITS(t->bits)
      if (e & 16)
      {
        /* get extra bits for length */
        e &= 15;
        c = t->base + ((uInt)b & inflate_mask[e]);
        DUMPBITS(e)
        Tracevv((stderr, "inflate:         * length %u\n", c));

        /* decode distance base of block to copy */
        GRABBITS(15);           /* max bits for distance code */
        e = (t = td + ((uInt)b & md))->exop;
        do {
          DUMPBITS(t->bits)
          if (e & 16)
          {
            /* get extra bits to add to distance base */
            e &= 15;
            GRABBITS(e)         /* get extra bits (up to 13) */
            d = t->base + ((uInt)b & inflate_mask[e]);
            DUMPBITS(e)
            Tracevv((stderr, "inflate:         * distance %u\n", d));

            /* do the copy */
            m -= c;
#if 1	/* { */
            if ((uInt)(q - s->window) >= d)     /* offset before dest */
            {                                   /*  just copy */
              r = q - d;
              *q++ = *r++;  c--;        /* minimum count is three, */
              *q++ = *r++;  c--;        /*  so unroll loop a little */
            }
            else                        /* else offset after destination */
            {
              e = d - (uInt)(q - s->window); /* bytes from offset to end */
              r = s->end - e;           /* pointer to offset */
              if (c > e)                /* if source crosses, */
              {
                c -= e;                 /* copy to end of window */
#ifdef __i386__
	{int delta = (int)q - (int)r;
	if (delta <= 0 || delta > 3) {
		quickmemcpy(q, r, e);
		q += e;
		e = 0;
		goto rest;
	}
	}
#endif
                do {
                  *q++ = *r++;
                } while (--e);
rest:
                r = s->window;          /* copy rest from start of window */
              }
            }
#ifdef __i386__
	{int delta = (int)q - (int)r;
	if (delta <= 0 || delta > 3) {
		quickmemcpy(q, r, c);
		q += c;
		r += c;
		c = 0;
		break;
	}
	}
#endif
            do {                        /* copy all or what's left */
              *q++ = *r++;
            } while (--c);
#endif	/* } */
            /*@innerbreak@*/ break;
          }
          else if ((e & 64) == 0)
          {
            t += t->base;
            e = (t += ((uInt)b & inflate_mask[e]))->exop;
		/*@innercontinue@*/ continue;
          }
          else
		goto inv_dist_code;
        } while (1);
/*@-unreachable@*/
        /*@innerbreak@*/ break;
/*@=unreachable@*/
      }
if (!(e & 64)) {

        t += t->base;
        if ((e = (t += ((uInt)b & inflate_mask[e]))->exop) != 0)
		/*@innercontinue@*/ continue;
        DUMPBITS(t->bits)
        Tracevv((stderr, t->base >= 0x20 && t->base < 0x7f ?
                  "inflate:         * literal '%c'\n" :
                  "inflate:         * literal 0x%02x\n", t->base));
        *q++ = (Byte)t->base;
        m--;
        /*@innerbreak@*/ break;
} else
{ uInt temp = e & 96;
      if (temp == 96)
	goto stream_end;
      if (temp == 64)
	goto data_error;
}
    } while (1);
  } while (m >= 258 && n >= 10);

  /* not enough input or output--restore pointers and return */
  ret = Z_OK;

ungrab:
        UNGRAB
        UPDATE
	return ret;

data_error:
      {
        z->msg = (char*)"invalid literal/length code";
        ret = Z_DATA_ERROR;
	goto ungrab;
      }

stream_end:
      {
        Tracevv((stderr, "inflate:         * end of block\n"));
        ret = Z_STREAM_END;
	goto ungrab;
      }

inv_dist_code:
          {
            z->msg = (char*)"invalid distance code";
            ret = Z_DATA_ERROR;
	    goto ungrab;
          }
}
