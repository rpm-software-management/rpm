#!/bin/bash 

echo 'rpmbuild testcase1'
rpmbuild -ba /data/project/isoft/kde/kjieba/kjieba.spec

echo 'rpmbuild testcase2'
rpmbuild --appstore --rebuild emacs-autopair-0.1-2.src.rpm

echo 'package is already installed testcase1'
rpm -e emacs-autopair
rpm -e emacs
rpm --appstore -e emacs-autopair
rpm --appstore -e emacs
rpm -ivh emacs-24.5-3.x86_64.rpm
rpm --appstore -ivh emacs-24.5-3.x86_64.rpm
rpm -q emacs
rpm --appstore -q emacs

echo 'package is already installed testcase2'
rpm -e emacs-autopair
rpm -e emacs
rpm --appstore -e emacs-autopair
rpm --appstore -e emacs
rpm --appstore -ivh emacs-24.5-3.x86_64.rpm
rpm -ivh emacs-24.5-3.x86_64.rpm
rpm -q emacs
rpm --appstore -q emacs

echo 'install app testcase'
rpm -e emacs-autopair
rpm -e emacs
rpm --appstore -e emacs-autopair
rpm --appstore -e emacs
rpm --appstore -ivh emacs-24.5-3.x86_64.rpm
rpm --appstore -ivh emacs-autopair-0.1-2.noarch.rpm

echo 'update app testcase'
rpm --appstore -e firefox
rpm --appstore -ivh firefox-41.0.2-15.x86_64.rpm
rpm --appstore -Uvh firefox-42.0-2.x86_64.rpm

echo 'yum testacse'
rm -rf /tmp/test
yum -c /etc/yum.repos.d/official-yum.repo --installroot /tmp/test install bash

echo 'verify app testcase'
rpm --appstore -Va

echo 'verify osdb testcase'
rpm -Va
