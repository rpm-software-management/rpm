#!/bin/sh

filelist=`sed "s/['\"]/\\\&/g"`

{	echo $filelist | tr [:blank:] \\n | /usr/lib/rpm/find-provides
#
# Generate perl module dependencies, if any.
    [ -x /usr/lib/rpm/perl.prov ] && \
	echo $filelist | tr [:blank:] \\n | /usr/lib/rpm/perl.prov
} | sort -u
