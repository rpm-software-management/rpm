#include "system.h"
#include "mp32barrett.h"
#include "mp32.h"
#include "popt.h"
#include "debug.h"

static int _debug = 0;

static int Zmp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	uint32  ysize = b->size+1;
	int ubits, vbits;
	int k = 0;

	uint32* u = wksp;
	uint32* v = u+ysize;
	uint32* A = v+ysize;
	uint32* B = A+ysize;
	uint32* C = B+ysize;
	uint32* D = C+ysize;

	mp32setx(ysize, u, xsize, xdata);
	mp32setx(ysize, v, b->size, b->modl);
	mp32setw(ysize, A, 1);
	mp32zero(ysize, B);
	mp32zero(ysize, C);
	mp32setw(ysize, D, 1);

#ifdef	NOTYET
	for (k = 0; mp32even(ysize, u) && mp32even(ysize, v); k++) {
		mp32divtwo(ysize, u);
		mp32divtwo(ysize, v);
	}

	if (mp32even(ysize, u))
		(void) mp32add(ysize, u, v);
#endif

if (_debug)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
if (_debug)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);

	ubits = vbits = 32 * (ysize);

	do {
		while (mp32even(ysize, v)) {
			mp32sdivtwo(ysize, v);
			vbits -= 1;
			if (mp32odd(ysize, C)) {
				(void) mp32addx(ysize, C, b->size, b->modl);
				(void) mp32subx(ysize, D, xsize, xdata);
			}
			mp32sdivtwo(ysize, C);
			mp32sdivtwo(ysize, D);
if (_debug)
fprintf(stderr, "-->>   v: "), mp32println(stderr, ysize, v);
		}

		if (ubits >= vbits) {
			uint32* swapu;
			uint32  swapi;

if (_debug)
fprintf(stderr, "--> (swap u <-> v)\n");
			swapu = u;	u = v;		v = swapu;
			swapi = ubits;	ubits = vbits;	vbits = swapi;
			swapu = A;	A = C;		C = swapu;
			swapu = B;	B = D;		D = swapu;
		}

		if (!((u[ysize-1] + v[ysize-1]) & 0x3)) {
if (_debug)
fprintf(stderr, "--> (even parity)\n");
			mp32add(ysize, v, u);
			mp32add(ysize, C, A);
			mp32add(ysize, D, B);
		} else {
if (_debug)
fprintf(stderr, "--> (odd parity)\n");
			mp32sub(ysize, v, u);
			mp32sub(ysize, C, A);
			mp32sub(ysize, D, B);
		}
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);
		vbits++;
	} while (mp32nz(ysize, v));

	if (result) {
#ifdef	NOTYET
		/*@-usedef@*/
		if (*A & 0x80000000)
			(void) mp32neg(ysize, A);
		/*@=usedef@*/
		while (--k > 0)
			mp32add(ysize, A, A);
#endif
		mp32setx(b->size, result, ysize, A);
	}

fprintf(stderr, "=== EXIT: "), mp32println(stderr, b->size, result);
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);

	return 1;
}

static int Ymp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	uint32  ysize = b->size+1;
 	int k;
	uint32* u1 = wksp;
	uint32* u2 = u1+ysize;
	uint32* u3 = u2+ysize;
	uint32* v1 = u3+ysize;
	uint32* v2 = v1+ysize;
	uint32* v3 = v2+ysize;
	uint32* t1 = v3+ysize;
	uint32* t2 = t1+ysize;
	uint32* t3 = t2+ysize;
	uint32* u  = t3+ysize;
	uint32* v  =  u+ysize;

	mp32setx(ysize, u, xsize, xdata);
	mp32setx(ysize, v, b->size, b->modl);

	/* Y1. Find power of 2. */
	for (k = 0; mp32even(ysize, u) && mp32even(ysize, v); k++) {
		mp32divtwo(ysize, u);
		mp32divtwo(ysize, v);
	}

if (_debug)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);

	/* Y2. Initialize. */
	mp32setw(ysize, u1, 1);
if (_debug)
fprintf(stderr, "      u1: "), mp32println(stderr, ysize, u1);
	mp32zero(ysize, u2);
if (_debug)
fprintf(stderr, "      u2: "), mp32println(stderr, ysize, u2);
	mp32setx(ysize, u3, ysize, u);
if (_debug)
fprintf(stderr, "      u3: "), mp32println(stderr, ysize, u3);

	mp32setx(ysize, v1, ysize, v);
if (_debug)
fprintf(stderr, "      v1: "), mp32println(stderr, ysize, v1);
	mp32setw(ysize, v2, 1);
	(void) mp32sub(ysize, v2, u);
if (_debug)
fprintf(stderr, "      v2: "), mp32println(stderr, ysize, v2);
	mp32setx(ysize, v3, ysize, v);
if (_debug)
fprintf(stderr, "      v3: "), mp32println(stderr, ysize, v3);

	if (mp32odd(ysize, u)) {
		mp32zero(ysize, t1);
if (_debug)
fprintf(stderr, "      t1: "), mp32println(stderr, ysize, t1);
		mp32zero(ysize, t2);
		mp32subw(ysize, t2, 1);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
		mp32zero(ysize, t3);
		mp32sub(ysize, t3, v);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);
		goto Y4;
	} else {
		mp32setw(ysize, t1, 1);
if (_debug)
fprintf(stderr, "      t1: "), mp32println(stderr, ysize, t1);
		mp32zero(ysize, t2);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
		mp32setx(ysize, t3, ysize, u);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);
	}

	do {
	    do {
		if (mp32odd(ysize, t1) || mp32odd(ysize, t2)) {
			mp32add(ysize, t1, v);
			mp32sub(ysize, t2, u);
		}
		mp32sdivtwo(ysize, t1);
		mp32sdivtwo(ysize, t2);
		mp32sdivtwo(ysize, t3);
Y4:
if (_debug)
fprintf(stderr, "   Y4 t3: "), mp32println(stderr, ysize, t3);
	    } while (mp32even(ysize, t3));

	    /* Y5. Reset max(u3,v3). */
	    if (!(*t3 & 0x80000000)) {
if (_debug)
fprintf(stderr, "--> Y5 (t3 > 0)\n");
		mp32setx(ysize, u1, ysize, t1);
if (_debug)
fprintf(stderr, "      u1: "), mp32println(stderr, ysize, u1);
		mp32setx(ysize, u2, ysize, t2);
if (_debug)
fprintf(stderr, "      u2: "), mp32println(stderr, ysize, u2);
		mp32setx(ysize, u3, ysize, t3);
if (_debug)
fprintf(stderr, "      u3: "), mp32println(stderr, ysize, u3);
	    } else {
if (_debug)
fprintf(stderr, "--> Y5 (t3 <= 0)\n");
		mp32setx(ysize, v1, ysize, v);
		mp32sub(ysize, v1, t1);
if (_debug)
fprintf(stderr, "      v1: "), mp32println(stderr, ysize, v1);
		mp32setx(ysize, v2, ysize, u);
		mp32neg(ysize, v2);
		mp32sub(ysize, v2, t2);
if (_debug)
fprintf(stderr, "      v2: "), mp32println(stderr, ysize, v2);
		mp32zero(ysize, v3);
		mp32sub(ysize, v3, t3);
if (_debug)
fprintf(stderr, "      v3: "), mp32println(stderr, ysize, v3);
	    }

	    /* Y6. Subtract. */
	    mp32setx(ysize, t1, ysize, u1);
	    mp32sub(ysize, t1, v1);
	    mp32setx(ysize, t2, ysize, u2);
	    mp32sub(ysize, t2, v2);
	    mp32setx(ysize, t3, ysize, u3);
	    mp32sub(ysize, t3, v3);

	    if (*t1 & 0x80000000) {
		mp32add(ysize, t1, v);
		mp32sub(ysize, t2, u);
	    }

if (_debug)
fprintf(stderr, "-->Y6 t1: "), mp32println(stderr, ysize, t1);
if (_debug)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
if (_debug)
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);

	} while (mp32nz(ysize, t3));

	if (result) {
		while (--k > 0)
			mp32add(ysize, u1, u1);
		mp32setx(b->size, result, ysize, u1);
	}

fprintf(stderr, "=== EXIT: "), mp32println(stderr, b->size, result);
fprintf(stderr, "      u1: "), mp32println(stderr, ysize, u1);
fprintf(stderr, "      u2: "), mp32println(stderr, ysize, u2);
fprintf(stderr, "      u3: "), mp32println(stderr, ysize, u3);
fprintf(stderr, "      v1: "), mp32println(stderr, ysize, v1);
fprintf(stderr, "      v2: "), mp32println(stderr, ysize, v2);
fprintf(stderr, "      v3: "), mp32println(stderr, ysize, v3);
fprintf(stderr, "      t1: "), mp32println(stderr, ysize, t1);
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);

	return 1;
}

/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 *  needs workspace of (6*size+6) words
 *  @note xdata and result cannot point to the same area
 */
static int Xmp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * Hence: if b->modl is even, then x must be odd, otherwise the gcd(x,n) >= 2
	 *
	 * The calling routine must guarantee this condition.
	 */

	uint32 ysize = b->size+1;

	uint32* u = wksp;
	uint32* v = u+ysize;
	uint32* A = v+ysize;
	uint32* B = A+ysize;
	uint32* C = B+ysize;
	uint32* D = C+ysize;

	mp32setx(ysize, u, b->size, b->modl);
	mp32setx(ysize, v, xsize, xdata);
	mp32setw(ysize, A, 1);
	mp32zero(ysize, B);
	mp32zero(ysize, C);
	mp32setw(ysize, D, 1);

if (_debug)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
if (_debug)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);

#ifdef	NOTNOW
	if (mp32odd(b->size, b->modl) && mp32even(xsize, xdata))
	{
		/* use simplified binary extended gcd algorithm */

		while (1)
		{
			while (mp32even(ysize, u))
			{
				mp32divtwo(ysize, u);

				if (mp32odd(ysize, B))
					(void) mp32subx(ysize, B, b->size, b->modl);

				mp32sdivtwo(ysize, B);
			}
			while (mp32even(ysize, v))
			{
				mp32divtwo(ysize, v);

				if (mp32odd(ysize, D))
					(void) mp32subx(ysize, D, b->size, b->modl);

				mp32sdivtwo(ysize, D);
			}
			if (mp32ge(ysize, u, v))
			{
				(void) mp32sub(ysize, u, v);
				(void) mp32sub(ysize, B, D);
			}
			else
			{
				(void) mp32sub(ysize, v, u);
				(void) mp32sub(ysize, D, B);
			}

			if (mp32z(ysize, u))
			{
				if (mp32isone(ysize, v))
				{
					if (result)
					{
						mp32setx(size, result, ysize, D);
						/*@-usedef@*/
						if (*D & 0x80000000)
							(void) mp32add(b->size, result, b->modl);
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
	else
#endif
	{
		/* use full binary extended gcd algorithm */

		while (1)
		{
			while (mp32even(ysize, u))
			{
				mp32divtwo(ysize, u);

				if (mp32odd(ysize, A) || mp32odd(ysize, B))
				{
					(void) mp32addx(ysize, A, xsize, xdata);
					(void) mp32subx(ysize, B, b->size, b->modl);
				}

				mp32sdivtwo(ysize, A);
				mp32sdivtwo(ysize, B);
			}
			while (mp32even(ysize, v))
			{
				mp32divtwo(ysize, v);

				if (mp32odd(ysize, C) || mp32odd(ysize, D))
				{
					(void) mp32addx(ysize, C, xsize, xdata);
					(void) mp32subx(ysize, D, b->size, b->modl);
				}

				mp32sdivtwo(ysize, C);
				mp32sdivtwo(ysize, D);
			}
			if (mp32ge(ysize, u, v))
			{
if (_debug)
fprintf(stderr, "--> 5 (u >= v)\n");
				(void) mp32sub(ysize, u, v);
				(void) mp32sub(ysize, A, C);
				(void) mp32sub(ysize, B, D);
if (_debug)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
			}
			else
			{
if (_debug)
fprintf(stderr, "--> 5 (u < v)\n");
				(void) mp32sub(ysize, v, u);
				(void) mp32sub(ysize, C, A);
				(void) mp32sub(ysize, D, B);
if (_debug)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);
			}

			if (mp32z(ysize, u))
			{
				if (mp32isone(ysize, v))
				{
					if (result)
					{
						mp32setx(b->size, result, ysize, D);
						/*@-usedef@*/
						if (*D & 0x80000000)
							(void) mp32add(b->size, result, b->modl);
						/*@=usedef@*/
					}
fprintf(stderr, "=== EXIT: "), mp32println(stderr, b->size, result);
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);
					return 1;
				}
				return 0;
			}
		}
	}
}

static const char * dsa_q	= "a1b35510319a59825c721e73e41d687ffe351bc9";
static const char * dsa_s[]	= {
    "0a73e678141aea7b4e5195afb7db3e9ec00f9f85",	/* time-1.7-14.i386.rpm */
    "259e4859e65c2528d3c35eaf2717d8963c834e94",	/* libxml2-2.4.2-1.i386.rpm */
    NULL
};

static const char * dsa_w_good[]= {
    "2659140a40cb05e85c536a299327addb0a762b8a",	/* time-1.7-14.i386.rpm */
    "6d1eaa6c78ad945a1de7bc369f7992e9df3735d9",	/* libxml2-2.4.2-1.i386.rpm */
    NULL
};

static const char * dsa_w_bad[]	= {
    "2659140a40cb05e85c536a299327addb0a762b8a",	/* time-1.7-14.i386.rpm */
    "cb6b555c47133ad7c1759dc2bb5c2a69e1021a10",	/* libxml2-2.4.2-1.i386.rpm */
    NULL
};

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char * argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    mp32barrett q;
    mp32number s;
    uint32 qsize;
    uint32* qtemp;
    uint32* qwksp;
    int rc;
    int i;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	default:
            /*@switchbreak@*/ break;
	}
    }

    mp32bzero(&q); mp32bsethex(&q, dsa_q);
    qsize = q.size;
    qtemp = malloc((13*qsize+13) * sizeof(*qtemp));
    qwksp = qtemp+2*qsize;

    for (i = 0; i < 1; i++) {
fprintf(stderr, "====================================\n");
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	mp32nzero(&s); mp32nsethex(&s, dsa_s[i]);

fprintf(stderr, "------------------------------------\n");
	rc = Xmp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "beecrypt: "); mp32println(stderr, qsize, qtemp);

#if 0
fprintf(stderr, "------------------------------------\n");
	rc = Ymp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Knuth: "); mp32println(stderr, qsize, qtemp);
#endif

fprintf(stderr, "------------------------------------\n");
	rc = Zmp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Brent: "); mp32println(stderr, qsize, qtemp);

fprintf(stderr, "------------------------------------\n");
	fprintf(stderr, "       q: %s\n", dsa_q);
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	fprintf(stderr, "    GOOD: %s\n", dsa_w_good[i]);
	fprintf(stderr, "     BAD: %s\n", dsa_w_bad[i]);
    }

    return 0;

}
