#!/bin/bash

# Setup the environment in the test-image.
# Runs INSIDE the image, with our newly built rpm in the path.

mkdir -p /build
ln -sf ../data/SOURCES /build/

# system-wide config to match our test environment
cp /data/macros.testenv @CMAKE_INSTALL_FULL_SYSCONFDIR@/rpm/

# setup an empty db that all tests are pointed to by default
rpmdb --initdb

# gpg-connect-agent is very, very unhappy if this doesn't exist
mkdir -p /root/.gnupg
chmod 700 /root/.gnupg

# set up new-style XDG config directory
mkdir -p /root/.config/rpm

