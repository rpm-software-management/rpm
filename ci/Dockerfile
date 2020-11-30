FROM registry.fedoraproject.org/fedora:31
MAINTAINER rpm-maint@lists.rpm.org

WORKDIR /srv/rpm
COPY . .

RUN echo -e "deltarpm=0\ninstall_weak_deps=0\ntsflags=nodocs" >> /etc/dnf/dnf.conf
RUN rm -f /etc/yum.repos.d/*modular.repo
# dummy for controlling per-repo gpgcheck via Semaphore setup
RUN sed -i -e "s:^gpgcheck=.$:gpgcheck=1:g" /etc/yum.repos.d/*.repo
RUN dnf -y update
RUN dnf -y install \
  autoconf \
  automake \
  libtool \
  gettext-devel \
  doxygen \
  make \
  gcc \
  git-core \
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
  lmdb-devel \
  libselinux-devel \
  ima-evm-utils-devel \
  libcap-devel \
  libacl-devel \
  audit-libs-devel \
  lua-devel readline-devel \
  python3-devel \
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
  --with-audit \
  --enable-python \
  --enable-silent-rules \
  --enable-werror
RUN make

CMD make distcheck; rc=$?; find . -name rpmtests.log|xargs cat; exit $rc
