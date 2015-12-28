#!/bin/bash 

echo 'rpmbuild testcase1'
rpmbuild -ba /data/project/isoft/kde/kjieba/kjieba.spec

echo 'rpmbuild testcase2'
rpmbuild --isoftapp --rebuild emacs-autopair-0.1-2.src.rpm

echo 'package is already installed testcase1'
rpm -e emacs-autopair
rpm -e emacs
rpm --isoftapp -e emacs-autopair
rpm --isoftapp -e emacs
rpm -ivh emacs-24.5-3.x86_64.rpm
rpm --isoftapp -ivh emacs-24.5-3.x86_64.rpm
rpm -q emacs
rpm --isoftapp -q emacs

echo 'package is already installed testcase2'
rpm -e emacs-autopair
rpm -e emacs
rpm --isoftapp -e emacs-autopair
rpm --isoftapp -e emacs
rpm --isoftapp -ivh emacs-24.5-3.x86_64.rpm
rpm -ivh emacs-24.5-3.x86_64.rpm
rpm -q emacs
rpm --isoftapp -q emacs

echo 'install app testcase'
rpm -e emacs-autopair
rpm -e emacs
rpm --isoftapp -e emacs-autopair
rpm --isoftapp -e emacs
rpm --isoftapp -ivh emacs-24.5-3.x86_64.rpm
rpm --isoftapp -ivh emacs-autopair-0.1-2.noarch.rpm

echo 'update app testcase'
rpm --isoftapp -e firefox
rpm --isoftapp -ivh firefox-41.0.2-15.x86_64.rpm
rpm --isoftapp -Uvh firefox-42.0-2.x86_64.rpm

echo 'yum testacse'
rm -rf /tmp/test
yum -c /etc/yum.repos.d/official-yum.repo --installroot /tmp/test install bash

echo 'verify app testcase'
rpm --isoftapp -Va

echo 'verify osdb testcase'
rpm -Va
