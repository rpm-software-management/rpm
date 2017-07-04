#!/bin/bash
# Ref: http://pkgs.fedoraproject.org/cgit/rpms/rpm.git/tree/rpm.spec

set -ex

DEFAULT_PREFIX='/usr'
DEFAULT_SYSCONFDIR='/etc'
DEFAULT_LOCALSTATEDIR='/var'
DEFAULT_SHAREDSTATEDIR='/var/lib'
DEFAULT_LIBDIR="$(rpm --eval '%{_libdir}')"
DEFAULT_TARGET_PLATFORM="$(rpm --eval '%{_build}')"

print_usage() {
    echo "Usage: $0 {install_dependency|build|check}" 1>&2
}

echo 'Building ...'

# To customize from outside.
PREFIX="${PREFIX:-${DEFAULT_PREFIX}}"
SYSCONFDIR="${SYSCONFDIR:-${DEFAULT_SYSCONFDIR}}"
LOCALSTATEDIR="${LOCALSTATEDIR:-${DEFAULT_LOCALSTATEDIR}}"
SHAREDSTATEDIR="${SHAREDSTATEDIR:-${DEFAULT_SHAREDSTATEDIR}}"
LIBDIR="${LIBDIR:-${DEFAULT_LIBDIR}}"
TARGET_PLATFORM="${TARGET_PLATFORM:-${DEFAULT_TARGET_PLATFORM}}"
DEFAULT_CONFIGURE_ARGS="
    --prefix=${PREFIX}
    --sysconfdir=${SYSCONFDIR}
    --localstatedir=${LOCALSTATEDIR}
    --sharedstatedir=${SHAREDSTATEDIR}
    --libdir=${LIBDIR}
    --build=${TARGET_PLATFORM}
    --host=${TARGET_PLATFORM}
    --with-vendor=redhat
    --with-external-db
    --with-lua
    --with-selinux
    --with-cap
    --with-acl
    --with-ndb
    --enable-python
    --with-crypto=openssl
"
CONFIGURE_ARGS="${CONFIGURE_ARGS:-${DEFAULT_CONFIGURE_ARGS}}"

CPPFLAGS="${CPPFLAGS} -DLUA_COMPAT_APIINTCASTS"
CFLAGS="${CFLAGS} -DLUA_COMPAT_APIINTCASTS"
LDFLAGS="${LDFLAGS} -L/${LIBDIR}/libdb"
export CPPFLAGS CFLAGS LDFLAGS

./autogen.sh --noconfigure
./configure ${CONFIGURE_ARGS}
make -j8
