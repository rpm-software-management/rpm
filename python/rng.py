import rpm

rpm.rng().Debug(1);
#rpm.mpw().Debug(-1);

# Generate random intergers
r = rpm.rng()
for i in range(100):
	print r.next()

# Generate random numbers with lots of bits
m = rpm.mpw("800000000000000000000000000000000000000000000000000000000000000000000000")
for i in range(100):
	r.next(m)

# Generate 160 bit random primes
bits = 160
trials = -1
a = r.prime(bits, trials)
for i in range(100):
	b = r.prime(bits, trials)
	print a.gcd(a,b)
	a = b
