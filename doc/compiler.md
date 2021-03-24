# What RPM expects from the C compiler

RPM expects some behavior from the C compiler that goes beyond what the
C standard guarantees.  Specifically:

- RPM assumes that the strict aliasing rule is not in effect.

- RPM assumes that `a + b`, where `a` is a pointer and `b` is an
  integer, is safe even if the resulting pointer is outside of the
  allocation `a` points to.  It may compare the resulting pointer, or
  perform pointer arithmetic on it.  It **will not** dereference an
  out-of-range pointer, of course.

- RPM assumes 2’s complement behavior on integer overflow.

RPM’s `configure` script trys to ensure that the compiler behaves
accordingly.  RPM passes `-fno-delete-null-pointer-checks`,
`-fno-strict-aliasing`, and `-fno-strict-overflow` to compilers that
support these flags.  This is sufficient for at least GCC.
