FROM registry.fedoraproject.org/fedora:34
MAINTAINER rpm-maint@lists.rpm.org

WORKDIR /srv/rpm

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
  debugedit \
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
  libgcrypt-devel \
  file-devel \
  popt-devel \
  libarchive-devel \
  sqlite-devel \
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
  fsverity-utils fsverity-utils-devel \
  pandoc \
  && dnf clean all

COPY . .

RUN autoreconf -vfi
RUN ./configure \
  --with-crypto=libgcrypt \
  --with-selinux \
  --with-cap \
  --with-acl \
  --with-audit \
  --with-fsverity \
  --enable-ndb \
  --enable-bdb-ro \
  --enable-sqlite \
  --enable-python \
  --enable-silent-rules \
  --enable-werror

# --enable-werror equivalent for Doxygen
RUN sed -i -e "/^WARN_AS_ERROR/s/ NO/ YES/g" docs/librpm.doxy.in

CMD make -j$(nproc) distcheck TESTSUITEFLAGS=-j$(nproc); rc=$?; find . -name rpmtests.log|xargs cat; exit $rc
