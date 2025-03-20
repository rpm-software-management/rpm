#!/bin/bash

# Setup the environment in the test-image.
# Runs INSIDE the image, with our newly built rpm in the path.

# Make sure nobody runs this accidentally outside the environment
if [ ! -f /.rpmtestsuite ]; then
    echo "Not inside rpm test-suite image"
    exit 99
fi

mkdir -p /build
ln -sf ../data/SOURCES /build/

# system-wide config to match our test environment
cp /data/macros.testenv @CMAKE_INSTALL_FULL_SYSCONFDIR@/rpm/

# setup an empty db that all tests are pointed to by default
rpmdb --initdb

# set up new-style XDG config directory
rpmhome=/root/.config/rpm
mkdir -p ${rpmhome}

# gpg-connect-agent is very, very unhappy if this doesn't exist
mkdir -p /root/.gnupg
chmod 700 /root/.gnupg

# setup default signing id + key (USER is not set when this runs!)
rpmconf=$(rpm --eval "%{_rpmconfigdir}")
USER=root ${rpmconf}/rpm-setup-autosign -p sq
rpmkeys --import /root/.config/rpm/*.asc
