#define partial_crc32		__z_partial_crc32
#define partial_crc32_copy	__z_partial_crc32_copy

#ifdef __i386__	/* { */
struct __crc32_fool_gcc {
	char	x[32768];
};

#if 0
#define PREFETCH(p)	__asm__ __volatile__("prefetch %0": : "m" (*(struct __crc32_fool_gcc *)(p)))
#else
#define PREFETCH(p)	(*(long *)(struct __crc32_fool_gcc *)(p))
#endif

#if 0
#define PREFETCH(p) do ; while (0)
#endif

static inline void partial_crc32_prep(uLong *crcp)
{
	*crcp ^= 0xffffffffL;
}

static inline void partial_crc32_finish(uLong *crcp)
{
	*crcp ^= 0xffffffffL;
}

static inline uLong get_crc_from_partial(uLong *crcp)
{
	return *crcp ^ 0xffffffffL;
}

extern const uLongf crc_table[256];

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);

static __inline__ uLong __partial_crc32(uLong crc, const Bytef *buf, uInt len, int copy, Bytef *dst)
{
	uInt n = len / 4;

#if 1	/* { */

#define DOlong(p, d) do { \
	int i; \
	crc ^= *(p); \
	if (copy) \
		*(d) = *(p); \
	crc = crc_table[crc & 0xff] ^ (crc >> 8); \
	crc = crc_table[crc & 0xff] ^ (crc >> 8); \
	crc = crc_table[crc & 0xff] ^ (crc >> 8); \
	crc = crc_table[crc & 0xff] ^ (crc >> 8); \
} while(0)

#if 0	/* { */
#define UPDcrc(x)	crc =
#define DOlong(p) \
	__asm__ __volatile__(	\
		"xorl	%1,%0;"	\
	  : "=a" (crc)	\
	  : "0" (crc)	\
	  : "ebx"	\
	)
#endif	/* } */

	if (n) {
		long *x = (long *)buf;
		long *y;
		int j = n / 8;

		PREFETCH(x);
		PREFETCH(x+8);
		PREFETCH(x+16);

		if (copy)
			y = (long *)dst;

		while (n >= 8) {
			PREFETCH(x+24);
			/* I hate gcc.  If I turn on loop unrolling,
			 * everything else is slowed down. */
			DOlong(x+0, y+0);
			DOlong(x+1, y+1);
			DOlong(x+2, y+2);
			DOlong(x+3, y+3);
			DOlong(x+4, y+4);
			DOlong(x+5, y+5);
			DOlong(x+6, y+6);
			DOlong(x+7, y+7);
			x += 8;
			y += 8;
			n -= 8;
		}

		while (n--) {
			DOlong(x, y);
			x++;
			y++;
		}

		buf = (Bytef *)x;
		if (copy)
			dst = (Bytef *)y;
	}
	len &= 3;
	while (len--) {
		if (copy)
			*dst++ = *buf;
		DO1(buf);
	}

#else /* } { */

	if (n) {
		do {
			DO4(buf);
		} while (--n);
	}

	len &= 3;
	switch(len) {
	case 3:
		if (copy)
			*dst++ = *buf;
		DO1(buf);
	case 2:
		if (copy)
			*dst++ = *buf;
		DO1(buf);
	case 1:
		if (copy)
			*dst++ = *buf;
		DO1(buf);
	}

#endif /* } */

	return crc;
}

#undef DO1
#undef DOlong

#if 0
#define partial_crc32(crc,buf,len) __partial_crc32(crc, buf, len, 0, 0)
#define partial_crc32_copy(crc,buf,len,dst) __partial_crc32((crc), (buf), (len), 1, (dst))
#endif

extern uLong partial_crc32(uLong crc, const Bytef *buf, uInt len) __attribute__((regparm(3)));
extern uLong partial_crc32_copy(uLong crcp, const Bytef *buf, uInt len, Bytef *dst) __attribute__((regparm(3)));

#endif /* } */
