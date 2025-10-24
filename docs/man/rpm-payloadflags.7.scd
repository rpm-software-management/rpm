RPM-PAYLOADFLAGS(7)

# NAME
*rpm-payloadflags* - RPM payload flags

# SYNOPSIS
*w*[_FLAGS_]._TYPE_

# DESCRIPTION
The payload flags determine how the payload of an RPM package is compressed
when the package is built. The flags are stored in the *Payloadcompressor*
and *Payloadflags* tags of the package header, which are used to determine
how to uncompress the payload contents during installation.

The peculiar syntax is directly inherited from librpm's *Fopen()* IO API,
where *w* means opening a file for writing.

The following compression types are supported, but availability may
vary depending on how RPM was compiled.

[- _TYPE_
:< Description
|  *ufdio*
:  uncompressed
|  *gzdio*
:  gzip (aka zlib)
|  *bzdio*
:  bzip2
|  *xzdio*
:  xz
|  *lzdio*
:  legacy lzma
|  *zstdio*
:  zstd

The compression _FLAGS_ must be listed in the following order and can be any of:

[- _FLAGS_
:< Description
:- Types
|  <0-9>
:  compression level
:  all (*ufdio* ignores)
|  T[0-N]
:  number of threads (no number or 0 = autodetect)
:  *xzdio*, *zstdio*
|  L<0-9>
:  window size(see *--long* in *zstd*(1))
:  *zstdio*

If a flag is omitted, the compressor's default value will be used.

A higher compression level generally means better compression ratio at the
cost of increased resource use and compression times.

*T* is equivalent to *T0* and causes the number of threads to be
automatically detected. Note that while using threads can speed up compression
considerably, it typically causes the compression ratio to go down, and
make the output less predictable.

# EXAMPLES
[[ Mode
:< Description
|  *w9.gzdio*
:  gzip level 9, default for package payload
|  *w9.bzdio*
:  bzip2 level 9, bzip2's default
|  *w.xzdio*
:  xz default level
|  *w7T16.xzdio*
:  xz level 7 using 16 threads
|  *w7T0.xzdio*
:  xz level 7, autodetect no. of threads
|  *w6.lzdio*
:  lzma (legacy) level 6, lzma's default
|  *w19T8.zstdio*
:  zstd level 19 using 8 threads
|  *w7T.zstdio*
:  zstd level 7, autodetect no. of threads
|  *w.ufdio*
:  uncompressed

# SEE ALSO
*rpmbuild-config*(5), *rpm-config*(5)

*http://www.rpm.org/*
