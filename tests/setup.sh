#!/bin/bash

# Setup the environment inside the test-image
# DESTDIR is only for testing purposes, inside the image it's always /
export DESTDIR=${1:-/}

mkdir -p $DESTDIR/build
ln -sf ../data/SOURCES $DESTDIR/build/

# setup an empty db that all tests are pointed to by default
dbpath="/var/lib/rpm-testsuite"
mkdir -p $DESTDIR/$dbpath
echo "%_dbpath $dbpath" > $DESTDIR/@CMAKE_INSTALL_FULL_SYSCONFDIR@/rpm/macros.db
rpmdb --dbpath $DESTDIR/$dbpath --initdb

# system-wide config to match our test environment
cp /data/macros.testenv $DESTDIR/@CMAKE_INSTALL_FULL_SYSCONFDIR@/rpm/

# gpg-connect-agent is very, very unhappy if this doesn't exist
mkdir -p $DESTDIR/root/.gnupg
chmod 700 $DESTDIR/root/.gnupg

# set up new-style XDG config directory
mkdir -p $DESTDIR/root/.config/rpm

