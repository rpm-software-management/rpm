FROM registry.fedoraproject.org/fedora:38
MAINTAINER rpm-maint@lists.rpm.org

RUN echo -e "deltarpm=0\ninstall_weak_deps=0\ntsflags=nodocs" >> /etc/dnf/dnf.conf
RUN rpm -e fedora-repos-modular
RUN sed -i -e "s:^enabled=.$:enabled=0:g" /etc/yum.repos.d/*openh264.repo
# dummy for controlling per-repo gpgcheck via Semaphore setup
RUN sed -i -e "s:^gpgcheck=.$:gpgcheck=1:g" /etc/yum.repos.d/*.repo
RUN dnf -y update
# until 1.4.0 lands in stable
RUN dnf -y --enablerepo=updates-testing install "rpm-sequoia-devel >= 1.4.0"
RUN dnf -y install \
  autoconf \
  cmake \
  gettext-devel \
  debugedit \
  doxygen \
  make \
  gcc \
  git-core \
  glibc-gconv-extra \
  zlib-devel \
  bzip2-devel \
  xz-devel \
  libzstd-devel \
  elfutils-libelf-devel \
  elfutils-devel \
  openssl-devel \
  libgcrypt-devel \
  rpm-sequoia-devel \
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
  which \
  elfutils binutils \
  findutils sed grep gawk diffutils file patch \
  tar unzip gzip bzip2 cpio xz \
  pkgconfig \
  /usr/bin/systemd-sysusers \
  /usr/bin/gdb-add-index \
  dwz \
  fsverity-utils fsverity-utils-devel \
  pandoc \
  && dnf clean all

WORKDIR /srv/rpm
COPY . .
RUN chmod -R a-w .

WORKDIR /srv/build
RUN cmake \
	-DCMAKE_INSTALL_PREFIX=$(mktemp -d) \
	-DENABLE_WERROR=ON \
	-DENABLE_BDB_RO=ON \
	-DWITH_FSVERITY=ON \
	-DWITH_IMAEVM=ON \
	../rpm

CMD make -j$(nproc) check; rc=$?; find . -name rpmtests.log|xargs cat; exit $rc
