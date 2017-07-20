FROM registry.fedoraproject.org/fedora

ENV TEST_USER_ID 1000

RUN dnf -y update

RUN dnf -y install \
    --setopt=tsflags=nodocs \
    --setopt=deltarpm=false \
    automake \
    binutils-devel \
    bzip2-devel \
    coreutils \
    curl \
    dbus-devel \
    elfutils-devel \
    elfutils-libelf-devel \
    fakechroot \
    file-devel \
    gawk \
    gettext-devel \
    ima-evm-utils \
    libacl-devel \
    libarchive-devel \
    libasan \
    libcap-devel \
    libdb-devel \
    libselinux-devel \
    libtool \
    libubsan \
    lua-devel \
    ncurses-devel \
    make \
    openssl-devel \
    popt \
    popt-devel \
    python3-devel \
    python-devel \
    readline-devel \
    redhat-rpm-config \
    rpm-build \
    which \
    xz-devel \
    zlib-devel

RUN dnf clean all

RUN useradd -u $TEST_USER_ID tester

WORKDIR /builddir

COPY . .

RUN chown -R 1000 .

USER $TEST_USER_ID

# Build RPM
# Ref: http://pkgs.fedoraproject.org/cgit/rpms/rpm.git/tree/rpm.spec
ENV LANG=en_US.UTF-8 \
    PREFIX='/usr' \
    SYSCONFDIR='/etc' \
    LOCALSTATEDIR='/var' \
    SHAREDSTATEDIR='/var/lib' \
    LIBDIR='/usr/lib64' \
    TARGET_PLATFORM='x86_64-redhat-linux-gnu'

RUN ./autogen.sh --noconfigure

RUN ./configure \
    --prefix=${PREFIX} \
    --sysconfdir=${SYSCONFDIR} \
    --localstatedir=${LOCALSTATEDIR} \
    --sharedstatedir=${SHAREDSTATEDIR} \
    --libdir=${LIBDIR} \
    --build=${TARGET_PLATFORM} \
    --host=${TARGET_PLATFORM} \
    --with-vendor=redhat \
    --with-external-db \
    --with-lua \
    --with-selinux \
    --with-cap \
    --with-acl \
    --with-ndb \
    --enable-python \
    --with-crypto=openssl

RUN make -j8

CMD make check
