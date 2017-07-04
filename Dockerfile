FROM fedora

ENV TEST_USER_ID 1000

RUN dnf -y install \
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
    zlib-devel \
    && dnf clean all

RUN useradd -u $TEST_USER_ID tester

ENV LANG=en_US.UTF-8

USER $TEST_USER_ID

CMD scripts/build.sh && make check
