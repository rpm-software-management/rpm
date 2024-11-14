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

# setup default signing id + key
sqemail="rpmbuild-user@$(uname -n)"
sqhome=${rpmhome}/sq
sqkey=$(sq key generate \
   --batch \
   --quiet \
   --own-key \
   --without-password \
   --can-sign \
   --cannot-authenticate \
   --cannot-encrypt \
   --email ${sqemail} \
      2>&1 | awk '/Fingerprint/{print $2}')

cat << EOF > ${rpmhome}/macros
%_openpgp_autosign_id ${sqkey}
%_openpgp_sign sq
EOF

# import the signing key by default
sq cert export \
    --cert-email "${sqemail}" > /root/rpm-key.asc
rpmkeys --import /root/rpm-key.asc

# gpg-connect-agent is very, very unhappy if this doesn't exist
mkdir -p /root/.gnupg
chmod 700 /root/.gnupg


