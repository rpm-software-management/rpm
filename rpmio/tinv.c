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

	for (k = 0; mp32even(ysize, u) && mp32even(ysize, v); k++) {
		mp32divtwo(ysize, u);
		mp32divtwo(ysize, v);
	}

	if (mp32even(ysize, u))
		(void) mp32add(ysize, u, v);

if (_debug < 0)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
if (_debug < 0)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug < 0)
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
if (_debug < 0)
fprintf(stderr, "-->>   v: "), mp32println(stderr, ysize, v);
		}

		if (ubits >= vbits) {
			uint32* swapu;
			uint32  swapi;

if (_debug < 0)
fprintf(stderr, "--> (swap u <-> v)\n");
			swapu = u;	u = v;		v = swapu;
			swapi = ubits;	ubits = vbits;	vbits = swapi;
			swapu = A;	A = C;		C = swapu;
			swapu = B;	B = D;		D = swapu;
		}

		if (!((u[ysize-1] + v[ysize-1]) & 0x3)) {
if (_debug < 0)
fprintf(stderr, "--> (even parity)\n");
			mp32add(ysize, v, u);
			mp32add(ysize, C, A);
			mp32add(ysize, D, B);
		} else {
if (_debug < 0)
fprintf(stderr, "--> (odd parity)\n");
			mp32sub(ysize, v, u);
			mp32sub(ysize, C, A);
			mp32sub(ysize, D, B);
		}
if (_debug < 0)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);
		vbits++;
	} while (mp32nz(ysize, v));

#ifdef	NOTYET
	if (!mp32isone(ysize, u))
		return 0;
#endif

	if (result) {
		mp32setx(b->size, result, ysize, A);
		/*@-usedef@*/
		if (*A & 0x80000000)
			(void) mp32neg(b->size, result);
		/*@=usedef@*/
		while (--k > 0)
			mp32add(b->size, result, result);
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

if (_debug < 0)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);

	/* Y2. Initialize. */
	mp32setw(ysize, u1, 1);
if (_debug < 0)
fprintf(stderr, "      u1: "), mp32println(stderr, ysize, u1);
	mp32zero(ysize, u2);
if (_debug < 0)
fprintf(stderr, "      u2: "), mp32println(stderr, ysize, u2);
	mp32setx(ysize, u3, ysize, u);
if (_debug < 0)
fprintf(stderr, "      u3: "), mp32println(stderr, ysize, u3);

	mp32setx(ysize, v1, ysize, v);
if (_debug < 0)
fprintf(stderr, "      v1: "), mp32println(stderr, ysize, v1);
	mp32setw(ysize, v2, 1);
	(void) mp32sub(ysize, v2, u);
if (_debug < 0)
fprintf(stderr, "      v2: "), mp32println(stderr, ysize, v2);
	mp32setx(ysize, v3, ysize, v);
if (_debug < 0)
fprintf(stderr, "      v3: "), mp32println(stderr, ysize, v3);

	if (mp32odd(ysize, u)) {
		mp32zero(ysize, t1);
if (_debug < 0)
fprintf(stderr, "      t1: "), mp32println(stderr, ysize, t1);
		mp32zero(ysize, t2);
		mp32subw(ysize, t2, 1);
if (_debug < 0)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
		mp32zero(ysize, t3);
		mp32sub(ysize, t3, v);
if (_debug < 0)
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);
		goto Y4;
	} else {
		mp32setw(ysize, t1, 1);
if (_debug < 0)
fprintf(stderr, "      t1: "), mp32println(stderr, ysize, t1);
		mp32zero(ysize, t2);
if (_debug < 0)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
		mp32setx(ysize, t3, ysize, u);
if (_debug < 0)
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
if (_debug < 0)
fprintf(stderr, "   Y4 t3: "), mp32println(stderr, ysize, t3);
	    } while (mp32even(ysize, t3));

	    /* Y5. Reset max(u3,v3). */
	    if (!(*t3 & 0x80000000)) {
if (_debug < 0)
fprintf(stderr, "--> Y5 (t3 > 0)\n");
		mp32setx(ysize, u1, ysize, t1);
if (_debug < 0)
fprintf(stderr, "      u1: "), mp32println(stderr, ysize, u1);
		mp32setx(ysize, u2, ysize, t2);
if (_debug < 0)
fprintf(stderr, "      u2: "), mp32println(stderr, ysize, u2);
		mp32setx(ysize, u3, ysize, t3);
if (_debug < 0)
fprintf(stderr, "      u3: "), mp32println(stderr, ysize, u3);
	    } else {
if (_debug < 0)
fprintf(stderr, "--> Y5 (t3 <= 0)\n");
		mp32setx(ysize, v1, ysize, v);
		mp32sub(ysize, v1, t1);
if (_debug < 0)
fprintf(stderr, "      v1: "), mp32println(stderr, ysize, v1);
		mp32setx(ysize, v2, ysize, u);
		mp32neg(ysize, v2);
		mp32sub(ysize, v2, t2);
if (_debug < 0)
fprintf(stderr, "      v2: "), mp32println(stderr, ysize, v2);
		mp32zero(ysize, v3);
		mp32sub(ysize, v3, t3);
if (_debug < 0)
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

if (_debug < 0)
fprintf(stderr, "-->Y6 t1: "), mp32println(stderr, ysize, t1);
if (_debug < 0)
fprintf(stderr, "      t2: "), mp32println(stderr, ysize, t2);
if (_debug < 0)
fprintf(stderr, "      t3: "), mp32println(stderr, ysize, t3);

	} while (mp32nz(ysize, t3));

	if (!(mp32isone(ysize, u3) && mp32isone(ysize, v3)))
		return 0;

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

if (_debug < 0)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
if (_debug < 0)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);

	do {
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
if (_debug < 0)
fprintf(stderr, "--> 5 (u >= v)\n");
			(void) mp32sub(ysize, u, v);
			(void) mp32sub(ysize, A, C);
			(void) mp32sub(ysize, B, D);
if (_debug < 0)
fprintf(stderr, "       u: "), mp32println(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       A: "), mp32println(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mp32println(stderr, ysize, B);
		}
		else
		{
if (_debug < 0)
fprintf(stderr, "--> 5 (u < v)\n");
			(void) mp32sub(ysize, v, u);
			(void) mp32sub(ysize, C, A);
			(void) mp32sub(ysize, D, B);
if (_debug < 0)
fprintf(stderr, "       v: "), mp32println(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       C: "), mp32println(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mp32println(stderr, ysize, D);
		}

	} while (mp32nz(ysize, u));

	if (!mp32isone(ysize, v))
		return 0;

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

static const char * dsa_q	= "a1b35510319a59825c721e73e41d687ffe351bc9";
static const char * dsa_s[]	= {
    "22e917d8a47462c09748e00aebbab5fd93793495",	/* samba-2.2.1a-4.i386.rpm */
    "0476b30eb86899c6785fad4f7a62e43d59481273",	/* gtkhtml-devel-0.9.2-9.i386.rpm */
    "8adbca132a0e6a2d2ee5bb2cd837b350c9f8db42",	/* lha-1.00-17.i386.rpm */

    "026efa7a5a60d29921ec93f503b5c483d131d8c4",	/* jed-0.99.14-2.i386.rpm */
    "2e4ec3c986b5a1f8f77b0b9f911d4e1b0ed8d869",	/* ttfonts-zh_TW-2.11-5.noarch.rpm */

    "259e4859e65c2528d3c35eaf2717d8963c834e94",	/* libxml2-2.4.2-1.i386.rpm */
    "45462b3534c2ff7a13f232a4e6e4460c61b2e232",	/* slang-1.4.4-4.i386.rpm */
    "0a73e678141aea7b4e5195afb7db3e9ec00f9f85",	/* time-1.7-14.i386.rpm */
    NULL
};

static const char * dsa_w_good[]= {
    "8b2eeda5fd34067c248bc3262e28f5668e64500b",	/* samba-2.2.1a-4.i386.rpm */
    "98f6a05c5cc17c2e48faad178d2c21c0bcca694b",	/* gtkhtml-devel-0.9.2-9.i386.rpm */
    "8ec91350f3237ee249ea009143f692d4cc2f8d2e",	/* lha-1.00-17.i386.rpm */

    "7db9e81c6f60fdd29243f67b70af7d1d14c9c703",	/* jed-0.99.14-2.i386.rpm */
    "6bdc316aef981e45c47dabab904a31747d349eec",	/* ttfonts-zh_TW-2.11-5.noarch.rpm */

    "6d1eaa6c78ad945a1de7bc369f7992e9df3735d9",	/* libxml2-2.4.2-1.i386.rpm */
    "79dc6adee7817e7dc248cfeb4b358e933af6de01",	/* slang-1.4.4-4.i386.rpm */
    "2659140a40cb05e85c536a299327addb0a762b8a",	/* time-1.7-14.i386.rpm */
    NULL
};

static const char * dsa_w_bad[]	= {
    "e97b9895cb99acf9c819a4b24a0b8ce6902f3442",	/* samba-2.2.1a-4.i386.rpm */
    "f7434b4c2b2722abec888ea3a90eb940be954d82",	/* gtkhtml-devel-0.9.2-9.i386.rpm */
    "ed15be40c189255fed77e21d5fd92a54cdfa7165",	/* lha-1.00-17.i386.rpm */

    "dc06930c3dc6a45035d1d8078c92149d1694ab3a",	/* jed-0.99.14-2.i386.rpm */
    "ca28dc5abdfdc4c3680b8d37ac2cc8f47eff8323",	/* ttfonts-zh_TW-2.11-5.noarch.rpm */

    "cb6b555c47133ad7c1759dc2bb5c2a69e1021a10",	/* libxml2-2.4.2-1.i386.rpm */
    "d82915ceb5e724fb65d6b177671826133cc1c238",	/* slang-1.4.4-4.i386.rpm */
    "2659140a40cb05e85c536a299327addb0a762b8a",	/* time-1.7-14.i386.rpm */
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

    for (i = 0; i < 9; i++) {
	if (dsa_s[i] == NULL) break;
fprintf(stderr, "================================================== %d\n", i);
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	mp32nzero(&s); mp32nsethex(&s, dsa_s[i]);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Xmp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "beecrypt: "); mp32println(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Ymp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Knuth: "); mp32println(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Zmp32binv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Brent: "); mp32println(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	fprintf(stderr, "       q: %s\n", dsa_q);
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	fprintf(stderr, "    GOOD: %s\n", dsa_w_good[i]);
	fprintf(stderr, "     BAD: %s\n", dsa_w_bad[i]);
    }

    return 0;

}
