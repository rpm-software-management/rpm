FROM fedora
MAINTAINER Igor Gnatenko <i.gnatenko.brain@gmail.com>

WORKDIR /opt/rpm
COPY . .

RUN echo -e "deltarpm=0\ninstall_weak_deps=0\ntsflags=nodocs" >> /etc/dnf/dnf.conf
RUN dnf -y update
RUN dnf -y install \
  autoconf \
  automake \
  libtool \
  gettext-devel \
  make \
  gcc \
  zlib-devel \
  bzip2-devel \
  xz-devel \
  libzstd-devel \
  elfutils-libelf-devel \
  elfutils-devel \
  openssl-devel \
  file-devel \
  popt-devel \
  libarchive-devel \
  libdb-devel \
  libselinux-devel \
  ima-evm-utils \
  libcap-devel \
  libacl-devel \
  lua-devel readline-devel \
  dbus-devel \
  fakechroot which \
  elfutils binutils \
  findutils sed grep gawk diffutils file patch \
  tar unzip gzip bzip2 cpio xz \
  pkgconfig \
  /usr/bin/gdb-add-index \
  dwz \
  && dnf clean all
RUN autoreconf -vfi
RUN ./configure \
  --with-crypto=openssl \
  --with-selinux \
  --with-cap \
  --with-acl \
  --with-lua \
  --enable-silent-rules
RUN make

CMD make check; rc=$?; cat tests/rpmtests.log; exit $rc
