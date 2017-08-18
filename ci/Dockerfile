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
  lmdb-devel \
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
  python-devel \
  python3-devel \
  redhat-rpm-config \
  && dnf clean all
RUN python3 -m ensurepip \
    && python3 -m pip install --upgrade pip setuptools \
    && python3 -m pip install tox
RUN autoreconf -vfi
RUN ./configure \
  --with-crypto=openssl \
  --with-selinux \
  --with-cap \
  --with-acl \
  --with-lua \
  --enable-silent-rules
RUN make
RUN cd python \
  && python setup.py build \
  && python3 setup.py build
# Install RPM to use /usr/local/lib/rpm/rpmrc in tox test.
RUN make install

CMD make check && pushd python && tox && popd; rc=$?; \
  cat tests/rpmtests.log; exit $rc
