#include "system.h"
#include "mp32barrett.h"
#include "mp32.h"
#include "debug.h"

static int _debug = 1;

static int Xmp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	uint32   size = b->size;
	uint32* bmodl = b->modl;
 	int k;
	uint32* u1 = wksp;
	uint32* u2 = u1+size+1;
	uint32* u3 = u2+size+1;
	uint32* v1 = u3+size+1;
	uint32* v2 = v1+size+1;
	uint32* v3 = v2+size+1;
	uint32* t1 = v3+size+1;
	uint32* t2 = t1+size+1;
	uint32* t3 = t2+size+1;
	uint32* u  = t3+size+1;
	uint32* v  =  u+size+1;

	mp32setx(size+1, u, size, bmodl);
	mp32setx(size+1, v, xsize, xdata);

	/* Y1. Find power of 2. */
	k = 0;
	while (mp32even(size+1, u) && mp32even(size+1, v)) {
		mp32divtwo(size+1, u);
		mp32divtwo(size+1, v);
		k++;
	}
if (_debug)
fprintf(stderr, "       u: "), mp32println(stderr, size+1, u);
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, size+1, v);

	/* Y2. Initialize. */
	mp32setw(size+1, u1, 1);
if (_debug)
fprintf(stderr, "      u1: "), mp32println(stderr, size+1, u1);
	mp32zero(size+1, u2);
if (_debug)
fprintf(stderr, "      u2: "), mp32println(stderr, size+1, u2);
	mp32setx(size+1, u3, size+1, u);
if (_debug)
fprintf(stderr, "      u3: "), mp32println(stderr, size+1, u3);

	mp32setx(size+1, v1, size+1, v);
if (_debug)
fprintf(stderr, "      v1: "), mp32println(stderr, size+1, v1);
	mp32setw(size+1, v2, 1);
	(void) mp32sub(size+1, v2, u);
if (_debug)
fprintf(stderr, "      v2: "), mp32println(stderr, size+1, v2);
	mp32setx(size+1, v3, size+1, v);
if (_debug)
fprintf(stderr, "      v3: "), mp32println(stderr, size+1, v3);

	if (mp32odd(size+1, u)) {
		mp32zero(size+1, t1);
if (_debug)
fprintf(stderr, "      t1: "), mp32println(stderr, size+1, t1);
		mp32zero(size+1, t2);
		mp32subw(size+1, t2, 1);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, size+1, t2);
		mp32zero(size+1, t3);
		mp32sub(size+1, t3, v);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, size+1, t3);
		goto Y4;
	} else {
		mp32setw(size+1, t1, 1);
if (_debug)
fprintf(stderr, "      t1: "), mp32println(stderr, size+1, t1);
		mp32zero(size+1, t2);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, size+1, t2);
		mp32setx(size+1, t3, size+1, u);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, size+1, t3);
	}

	do {
	    do {
		if (!(mp32even(size+1, t1) && mp32even(size+1, t2))) {
			mp32add(size+1, t1, v);
			mp32sub(size+1, t2, u);
		}
		mp32sdivtwo(size+1, t1);
		mp32sdivtwo(size+1, t2);
		mp32sdivtwo(size+1, t3);
Y4:
if (_debug)
fprintf(stderr, "   Y4 t3: "), mp32println(stderr, size+1, t3);
	    } while (mp32even(size+1, t3));

	    /* Y5. Reset max(u3,v3). */
	    if (!mp32msbset(size+1, t3)) {
if (_debug)
fprintf(stderr, "--> Y5 (t3 > 0)\n");
		mp32setx(size+1, u1, size+1, t1);
if (_debug)
fprintf(stderr, "      u1: "), mp32println(stderr, size+1, u1);
		mp32setx(size+1, u2, size+1, t2);
if (_debug)
fprintf(stderr, "      u2: "), mp32println(stderr, size+1, u2);
		mp32setx(size+1, u3, size+1, t3);
if (_debug)
fprintf(stderr, "      u3: "), mp32println(stderr, size+1, u3);
	    } else {
if (_debug)
fprintf(stderr, "--> Y5 (t3 <= 0)\n");
		mp32setx(size+1, v1, size+1, v);
		mp32sub(size+1, v1, t1);
if (_debug)
fprintf(stderr, "      v1: "), mp32println(stderr, size+1, v1);
		mp32setx(size+1, v2, size+1, u);
		mp32neg(size+1, v2);
		mp32sub(size+1, v2, t2);
if (_debug)
fprintf(stderr, "      v2: "), mp32println(stderr, size+1, v2);
		mp32zero(size+1, v3);
		mp32sub(size+1, v3, t3);
if (_debug)
fprintf(stderr, "      v3: "), mp32println(stderr, size+1, v3);
	    }

	    /* Y6. Subtract. */
	    mp32setx(size+1, t1, size+1, u1);
	    mp32sub(size+1, t1, v1);
	    mp32setx(size+1, t2, size+1, u2);
	    mp32sub(size+1, t2, v2);
	    mp32setx(size+1, t3, size+1, u3);
	    mp32sub(size+1, t3, v3);

	    if (*t1 & 0x80000000) {
		mp32add(size+1, t1, v);
		mp32sub(size+1, t2, u);
	    }

if (_debug)
fprintf(stderr, "-->Y6 t1: "), mp32println(stderr, size+1, t1);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, size+1, t2);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, size+1, t3);

	} while (mp32nz(size+1, t3));

fprintf(stderr, "==== EXIT\n");
fprintf(stderr, "      u1: "); mp32println(stderr, size+1, u1);
fprintf(stderr, "      u2: "); mp32println(stderr, size+1, u2);
fprintf(stderr, "      u3: "); mp32println(stderr, size+1, u3);
fprintf(stderr, "      v1: "); mp32println(stderr, size+1, v1);
fprintf(stderr, "      v2: "); mp32println(stderr, size+1, v2);
fprintf(stderr, "      v3: "); mp32println(stderr, size+1, v3);
fprintf(stderr, "      t1: "); mp32println(stderr, size+1, t1);
fprintf(stderr, "      t2: "); mp32println(stderr, size+1, t2);
fprintf(stderr, "      t3: "); mp32println(stderr, size+1, t3);

	if (result) {
		mp32setx(size, result, size+1, u2);
		while (--k > 0)
			mp32add(size, result, result);
	}

	return 1;
}

/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 *  needs workspace of (6*size+6) words
 *  @note xdata and result cannot point to the same area
 */
static int mp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * Hence: if b->modl is even, then x must be odd, otherwise the gcd(x,n) >= 2
	 *
	 * The calling routine must guarantee this condition.
	 */

	register uint32  size = b->size;
	uint32* bmodl = b->modl;

	uint32* udata = wksp;
	uint32* vdata = udata+size+1;
	uint32* adata = vdata+size+1;
	uint32* bdata = adata+size+1;
	uint32* cdata = bdata+size+1;
	uint32* ddata = cdata+size+1;

	mp32setx(size+1, udata, size, bmodl);
	mp32setx(size+1, vdata, xsize, xdata);
	mp32zero(size+1, bdata);
	mp32setw(size+1, ddata, 1);

	if (mp32odd(size, bmodl) && mp32even(xsize, xdata))
	{
		/* use simplified binary extended gcd algorithm */

		while (1)
		{
			while (mp32even(size+1, udata))
			{
				mp32divtwo(size+1, udata);

				if (mp32odd(size+1, bdata))
					(void) mp32subx(size+1, bdata, size, bmodl);

				mp32sdivtwo(size+1, bdata);
			}
			while (mp32even(size+1, vdata))
			{
				mp32divtwo(size+1, vdata);

				if (mp32odd(size+1, ddata))
					(void) mp32subx(size+1, ddata, size, bmodl);

				mp32sdivtwo(size+1, ddata);
			}
			if (mp32ge(size+1, udata, vdata))
			{
				(void) mp32sub(size+1, udata, vdata);
				(void) mp32sub(size+1, bdata, ddata);
			}
			else
			{
				(void) mp32sub(size+1, vdata, udata);
				(void) mp32sub(size+1, ddata, bdata);
			}

			if (mp32z(size+1, udata))
			{
				if (mp32isone(size+1, vdata))
				{
					if (result)
					{
						mp32setx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & 0x80000000)
							(void) mp32add(size, result, bmodl);
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
	else
	{
		/* use full binary extended gcd algorithm */
		mp32setw(size+1, adata, 1);
		mp32zero(size+1, cdata);

		while (1)
		{
			while (mp32even(size+1, udata))
			{
				mp32divtwo(size+1, udata);

				if (mp32odd(size+1, adata) || mp32odd(size+1, bdata))
				{
					(void) mp32addx(size+1, adata, xsize, xdata);
					(void) mp32subx(size+1, bdata, size, bmodl);
				}

				mp32sdivtwo(size+1, adata);
				mp32sdivtwo(size+1, bdata);
			}
			while (mp32even(size+1, vdata))
			{
				mp32divtwo(size+1, vdata);

				if (mp32odd(size+1, cdata) || mp32odd(size+1, ddata))
				{
					(void) mp32addx(size+1, cdata, xsize, xdata);
					(void) mp32subx(size+1, ddata, size, bmodl);
				}

				mp32sdivtwo(size+1, cdata);
				mp32sdivtwo(size+1, ddata);
			}
			if (mp32ge(size+1, udata, vdata))
			{
				(void) mp32sub(size+1, udata, vdata);
				(void) mp32sub(size+1, adata, cdata);
				(void) mp32sub(size+1, bdata, ddata);
			}
			else
			{
				(void) mp32sub(size+1, vdata, udata);
				(void) mp32sub(size+1, cdata, adata);
				(void) mp32sub(size+1, ddata, bdata);
			}

			if (mp32z(size+1, udata))
			{
				if (mp32isone(size+1, vdata))
				{
					if (result)
					{
						mp32setx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & 0x80000000)
							(void) mp32add(size, result, bmodl);
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
}

static const char * dsa_q	= "a1b35510319a59825c721e73e41d687ffe351bc9";
static const char * dsa_s	= "259e4859e65c2528d3c35eaf2717d8963c834e94";

#if 1
static const char * dsa_w_good	= "6d1eaa6c78ad945a1de7bc369f7992e9df3735d9";
static const char * dsa_w_bad	= "cb6b555c47133ad7c1759dc2bb5c2a69e1021a10";
#endif

#if 0
static const char * dsa_w_good	= "79dc6adee7817e7dc248cfeb4b358e933af6de01";
static const char * dsa_w_bad	= "d82915ceb5e724fb65d6b177671826133cc1c238";
#endif

int
main(int argc, const char * argv[])
{
    mp32barrett q;
    mp32number s;
    uint32 qsize;
    uint32* qtemp;
    uint32* qwksp;
    int rc;

    mp32bzero(&q); mp32bsethex(&q, dsa_q);
    mp32nzero(&s); mp32nsethex(&s, dsa_s);
    qsize = q.size;

    qtemp = malloc((11*qsize+11) * sizeof(*qtemp));
    qwksp = qtemp+2*qsize;

    fprintf(stderr, "       q: %s\n", dsa_q);
    fprintf(stderr, "       s: %s\n", dsa_s);
    fprintf(stderr, "    GOOD: %s\n", dsa_w_good);
    fprintf(stderr, "     BAD: %s\n", dsa_w_bad);

    rc = mp32binv_w(&q, s.size, s.data, qtemp, qwksp);
    fprintf(stderr, "beecrypt: "); mp32println(stderr, qsize, qtemp);

    rc = Xmp32binv_w(&q, s.size, s.data, qtemp, qwksp);
    fprintf(stderr, "   Knuth: "); mp32println(stderr, qsize, qtemp);

    return 0;

}
