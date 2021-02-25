#!/bin/sh --
set -eu
if [ $# -lt 1 ]; then
    echo 'Usage: fuzz-header.sh <build-dir> <fuzzer-args>'>&2
    exit 1
fi

q=$(realpath -e -- "$0")
q=${q%/*} dir=$1
shift
mkdir -p -m 0700 -- "$dir"
cd -- "$dir"
# if [ -f Makefile ]; then make distclean; fi
"$q/configure" --with-crypto=openssl \
  --with-selinux \
  --with-cap \
  --with-acl \
  --with-audit \
  --without-fsverity \
  --enable-ndb \
  --enable-bdb-ro \
  --enable-sqlite \
  --enable-python \
  --enable-silent-rules \
  --enable-static \
  --enable-werror \
  --cache=config.cache \
  CC=clang CPPFLAGS=-DRPM_HEADER_LIBFUZZER=1

make CCLD=/bin/true CPPFLAGS=-DRPM_HEADER_LIBFUZZER=1 CFLAGS='-O2 -D_FORTIFY_SOURCE=2 -fPIC -fsanitize=address,undefined,fuzzer,unsigned-integer-overflow,local-bounds,nullability,implicit-conversion,float-divide-by-zero -g3 -Werror -Wno-enum-conversion -Wno-unused-function -Wno-error=unused-result -Wno-format-invalid-specifier'

clang -fsanitize=address,undefined,fuzzer \
    "$dir/lib/header.o" \
    -Xlinker --start-group \
    "$dir/rpmio/.libs/librpmio.a" \
    "$dir/lib/.libs/librpm.a" \
    -o "$q/fuzz-header" \
    -llua -lzstd -llzma -lbz2 -lz -lcrypto -lpopt -lsqlite3 -ldb -lmagic -lelf -ldw -ldb -lcap -lacl -laudit \
    -Xlinker --end-group

cd -- "$q"
"./fuzz-header" "$@"
