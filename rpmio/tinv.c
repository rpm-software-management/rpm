#include "system.h"
#include "mpbarrett.h"
#include "mp.h"
#include "popt.h"
#include "debug.h"

static int _debug = 0;

static int Zmpbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	size_t ysize = b->size+1;
	size_t ubits, vbits;
	int k = 0;

	mpw* u = wksp;
	mpw* v = u+ysize;
	mpw* A = v+ysize;
	mpw* B = A+ysize;
	mpw* C = B+ysize;
	mpw* D = C+ysize;

	mpsetx(ysize, u, xsize, xdata);
	mpsetx(ysize, v, b->size, b->modl);
	mpsetw(ysize, A, 1);
	mpzero(ysize, B);
	mpzero(ysize, C);
	mpsetw(ysize, D, 1);

	for (k = 0; mpeven(ysize, u) && mpeven(ysize, v); k++) {
		mpdivtwo(ysize, u);
		mpdivtwo(ysize, v);
	}

	if (mpeven(ysize, u))
		(void) mpadd(ysize, u, v);

if (_debug < 0)
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       A: "), mpfprintln(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mpfprintln(stderr, ysize, B);
if (_debug < 0)
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);

	ubits = vbits = MP_WORDS_TO_BITS(ysize);

	do {
		while (mpeven(ysize, v)) {
			mpsdivtwo(ysize, v);
			vbits -= 1;
			if (mpodd(ysize, C)) {
				(void) mpaddx(ysize, C, b->size, b->modl);
				(void) mpsubx(ysize, D, xsize, xdata);
			}
			mpsdivtwo(ysize, C);
			mpsdivtwo(ysize, D);
if (_debug < 0)
fprintf(stderr, "-->>   v: "), mpfprintln(stderr, ysize, v);
		}

		if (ubits >= vbits) {
			mpw* swapu;
			size_t  swapi;

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
			mpadd(ysize, v, u);
			mpadd(ysize, C, A);
			mpadd(ysize, D, B);
		} else {
if (_debug < 0)
fprintf(stderr, "--> (odd parity)\n");
			mpsub(ysize, v, u);
			mpsub(ysize, C, A);
			mpsub(ysize, D, B);
		}
if (_debug < 0)
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);
		vbits++;
	} while (mpnz(ysize, v));

#ifdef	NOTYET
	if (!mpisone(ysize, u))
		return 0;
#endif

	if (result) {
		mpsetx(b->size, result, ysize, A);
		/*@-usedef@*/
		if (*A & 0x80000000)
			(void) mpneg(b->size, result);
		/*@=usedef@*/
		while (--k > 0)
			mpadd(b->size, result, result);
	}

fprintf(stderr, "=== EXIT: "), mpfprintln(stderr, b->size, result);
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
fprintf(stderr, "       A: "), mpfprintln(stderr, ysize, A);
fprintf(stderr, "       B: "), mpfprintln(stderr, ysize, B);
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);

	return 1;
}

static int Ympbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	size_t  ysize = b->size+1;
 	int k;
	mpw* u1 = wksp;
	mpw* u2 = u1+ysize;
	mpw* u3 = u2+ysize;
	mpw* v1 = u3+ysize;
	mpw* v2 = v1+ysize;
	mpw* v3 = v2+ysize;
	mpw* t1 = v3+ysize;
	mpw* t2 = t1+ysize;
	mpw* t3 = t2+ysize;
	mpw* u  = t3+ysize;
	mpw* v  =  u+ysize;

	mpsetx(ysize, u, xsize, xdata);
	mpsetx(ysize, v, b->size, b->modl);

	/* Y1. Find power of 2. */
	for (k = 0; mpeven(ysize, u) && mpeven(ysize, v); k++) {
		mpdivtwo(ysize, u);
		mpdivtwo(ysize, v);
	}

if (_debug < 0)
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);

	/* Y2. Initialize. */
	mpsetw(ysize, u1, 1);
if (_debug < 0)
fprintf(stderr, "      u1: "), mpfprintln(stderr, ysize, u1);
	mpzero(ysize, u2);
if (_debug < 0)
fprintf(stderr, "      u2: "), mpfprintln(stderr, ysize, u2);
	mpsetx(ysize, u3, ysize, u);
if (_debug < 0)
fprintf(stderr, "      u3: "), mpfprintln(stderr, ysize, u3);

	mpsetx(ysize, v1, ysize, v);
if (_debug < 0)
fprintf(stderr, "      v1: "), mpfprintln(stderr, ysize, v1);
	mpsetw(ysize, v2, 1);
	(void) mpsub(ysize, v2, u);
if (_debug < 0)
fprintf(stderr, "      v2: "), mpfprintln(stderr, ysize, v2);
	mpsetx(ysize, v3, ysize, v);
if (_debug < 0)
fprintf(stderr, "      v3: "), mpfprintln(stderr, ysize, v3);

	if (mpodd(ysize, u)) {
		mpzero(ysize, t1);
if (_debug < 0)
fprintf(stderr, "      t1: "), mpfprintln(stderr, ysize, t1);
		mpzero(ysize, t2);
		mpsubw(ysize, t2, 1);
if (_debug < 0)
fprintf(stderr, "      t2: "), mpfprintln(stderr, ysize, t2);
		mpzero(ysize, t3);
		mpsub(ysize, t3, v);
if (_debug < 0)
fprintf(stderr, "      t3: "), mpfprintln(stderr, ysize, t3);
		goto Y4;
	} else {
		mpsetw(ysize, t1, 1);
if (_debug < 0)
fprintf(stderr, "      t1: "), mpfprintln(stderr, ysize, t1);
		mpzero(ysize, t2);
if (_debug < 0)
fprintf(stderr, "      t2: "), mpfprintln(stderr, ysize, t2);
		mpsetx(ysize, t3, ysize, u);
if (_debug < 0)
fprintf(stderr, "      t3: "), mpfprintln(stderr, ysize, t3);
	}

	do {
	    do {
		if (mpodd(ysize, t1) || mpodd(ysize, t2)) {
			mpadd(ysize, t1, v);
			mpsub(ysize, t2, u);
		}
		mpsdivtwo(ysize, t1);
		mpsdivtwo(ysize, t2);
		mpsdivtwo(ysize, t3);
Y4:
if (_debug < 0)
fprintf(stderr, "   Y4 t3: "), mpfprintln(stderr, ysize, t3);
	    } while (mpeven(ysize, t3));

	    /* Y5. Reset max(u3,v3). */
	    if (!(*t3 & 0x80000000)) {
if (_debug < 0)
fprintf(stderr, "--> Y5 (t3 > 0)\n");
		mpsetx(ysize, u1, ysize, t1);
if (_debug < 0)
fprintf(stderr, "      u1: "), mpfprintln(stderr, ysize, u1);
		mpsetx(ysize, u2, ysize, t2);
if (_debug < 0)
fprintf(stderr, "      u2: "), mpfprintln(stderr, ysize, u2);
		mpsetx(ysize, u3, ysize, t3);
if (_debug < 0)
fprintf(stderr, "      u3: "), mpfprintln(stderr, ysize, u3);
	    } else {
if (_debug < 0)
fprintf(stderr, "--> Y5 (t3 <= 0)\n");
		mpsetx(ysize, v1, ysize, v);
		mpsub(ysize, v1, t1);
if (_debug < 0)
fprintf(stderr, "      v1: "), mpfprintln(stderr, ysize, v1);
		mpsetx(ysize, v2, ysize, u);
		mpneg(ysize, v2);
		mpsub(ysize, v2, t2);
if (_debug < 0)
fprintf(stderr, "      v2: "), mpfprintln(stderr, ysize, v2);
		mpzero(ysize, v3);
		mpsub(ysize, v3, t3);
if (_debug < 0)
fprintf(stderr, "      v3: "), mpfprintln(stderr, ysize, v3);
	    }

	    /* Y6. Subtract. */
	    mpsetx(ysize, t1, ysize, u1);
	    mpsub(ysize, t1, v1);
	    mpsetx(ysize, t2, ysize, u2);
	    mpsub(ysize, t2, v2);
	    mpsetx(ysize, t3, ysize, u3);
	    mpsub(ysize, t3, v3);

	    if (*t1 & 0x80000000) {
		mpadd(ysize, t1, v);
		mpsub(ysize, t2, u);
	    }

if (_debug < 0)
fprintf(stderr, "-->Y6 t1: "), mpfprintln(stderr, ysize, t1);
if (_debug < 0)
fprintf(stderr, "      t2: "), mpfprintln(stderr, ysize, t2);
if (_debug < 0)
fprintf(stderr, "      t3: "), mpfprintln(stderr, ysize, t3);

	} while (mpnz(ysize, t3));

	if (!(mpisone(ysize, u3) && mpisone(ysize, v3)))
		return 0;

	if (result) {
		while (--k > 0)
			mpadd(ysize, u1, u1);
		mpsetx(b->size, result, ysize, u1);
	}

fprintf(stderr, "=== EXIT: "), mpfprintln(stderr, b->size, result);
fprintf(stderr, "      u1: "), mpfprintln(stderr, ysize, u1);
fprintf(stderr, "      u2: "), mpfprintln(stderr, ysize, u2);
fprintf(stderr, "      u3: "), mpfprintln(stderr, ysize, u3);
fprintf(stderr, "      v1: "), mpfprintln(stderr, ysize, v1);
fprintf(stderr, "      v2: "), mpfprintln(stderr, ysize, v2);
fprintf(stderr, "      v3: "), mpfprintln(stderr, ysize, v3);
fprintf(stderr, "      t1: "), mpfprintln(stderr, ysize, t1);
fprintf(stderr, "      t2: "), mpfprintln(stderr, ysize, t2);
fprintf(stderr, "      t3: "), mpfprintln(stderr, ysize, t3);

	return 1;
}

/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 *  needs workspace of (6*size+6) words
 *  @note xdata and result cannot point to the same area
 */
static int Xmpbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * Hence: if b->modl is even, then x must be odd, otherwise the gcd(x,n) >= 2
	 *
	 * The calling routine must guarantee this condition.
	 */

	size_t ysize = b->size+1;

	mpw* u = wksp;
	mpw* v = u+ysize;
	mpw* A = v+ysize;
	mpw* B = A+ysize;
	mpw* C = B+ysize;
	mpw* D = C+ysize;

	mpsetx(ysize, u, b->size, b->modl);
	mpsetx(ysize, v, xsize, xdata);
	mpsetw(ysize, A, 1);
	mpzero(ysize, B);
	mpzero(ysize, C);
	mpsetw(ysize, D, 1);

if (_debug < 0)
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       A: "), mpfprintln(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mpfprintln(stderr, ysize, B);
if (_debug < 0)
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);

	do {
		while (mpeven(ysize, u))
		{
			mpdivtwo(ysize, u);

			if (mpodd(ysize, A) || mpodd(ysize, B))
			{
				(void) mpaddx(ysize, A, xsize, xdata);
				(void) mpsubx(ysize, B, b->size, b->modl);
			}

			mpsdivtwo(ysize, A);
			mpsdivtwo(ysize, B);
		}
		while (mpeven(ysize, v))
		{
			mpdivtwo(ysize, v);

			if (mpodd(ysize, C) || mpodd(ysize, D))
			{
				(void) mpaddx(ysize, C, xsize, xdata);
				(void) mpsubx(ysize, D, b->size, b->modl);
			}

			mpsdivtwo(ysize, C);
			mpsdivtwo(ysize, D);
		}
		if (mpge(ysize, u, v))
		{
if (_debug < 0)
fprintf(stderr, "--> 5 (u >= v)\n");
			(void) mpsub(ysize, u, v);
			(void) mpsub(ysize, A, C);
			(void) mpsub(ysize, B, D);
if (_debug < 0)
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
if (_debug < 0)
fprintf(stderr, "       A: "), mpfprintln(stderr, ysize, A);
if (_debug < 0)
fprintf(stderr, "       B: "), mpfprintln(stderr, ysize, B);
		}
		else
		{
if (_debug < 0)
fprintf(stderr, "--> 5 (u < v)\n");
			(void) mpsub(ysize, v, u);
			(void) mpsub(ysize, C, A);
			(void) mpsub(ysize, D, B);
if (_debug < 0)
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
if (_debug < 0)
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
if (_debug < 0)
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);
		}

	} while (mpnz(ysize, u));

	if (!mpisone(ysize, v))
		return 0;

	if (result)
	{
		mpsetx(b->size, result, ysize, D);
		/*@-usedef@*/
		if (*D & 0x80000000)
			(void) mpadd(b->size, result, b->modl);
		/*@=usedef@*/
	}

fprintf(stderr, "=== EXIT: "), mpfprintln(stderr, b->size, result);
fprintf(stderr, "       u: "), mpfprintln(stderr, ysize, u);
fprintf(stderr, "       v: "), mpfprintln(stderr, ysize, v);
fprintf(stderr, "       A: "), mpfprintln(stderr, ysize, A);
fprintf(stderr, "       B: "), mpfprintln(stderr, ysize, B);
fprintf(stderr, "       C: "), mpfprintln(stderr, ysize, C);
fprintf(stderr, "       D: "), mpfprintln(stderr, ysize, D);
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
    mpbarrett q;
    mpnumber s;
    size_t qsize;
    mpw* qtemp;
    mpw* qwksp;
    int rc;
    int i;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	default:
            /*@switchbreak@*/ break;
	}
    }

    mpbzero(&q); mpbsethex(&q, dsa_q);
    qsize = q.size;
    qtemp = malloc((13*qsize+13) * sizeof(*qtemp));
    qwksp = qtemp+2*qsize;

    for (i = 0; i < 9; i++) {
	if (dsa_s[i] == NULL) break;
fprintf(stderr, "================================================== %d\n", i);
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	mpnzero(&s); mpnsethex(&s, dsa_s[i]);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Xmpbinv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "beecrypt: "); mpfprintln(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Ympbinv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Knuth: "); mpfprintln(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	rc = Zmpbinv_w(&q, s.size, s.data, qtemp, qwksp);
	fprintf(stderr, "   Brent: "); mpfprintln(stderr, qsize, qtemp);

fprintf(stderr, "-------------------------------------------------- %d\n", i);
	fprintf(stderr, "       q: %s\n", dsa_q);
	fprintf(stderr, "       s: %s\n", dsa_s[i]);
	fprintf(stderr, "    GOOD: %s\n", dsa_w_good[i]);
	fprintf(stderr, "     BAD: %s\n", dsa_w_bad[i]);
    }

    return 0;

}
